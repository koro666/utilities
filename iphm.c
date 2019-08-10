#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

struct cidr_address
{
	uint32_t address;
	uint32_t count;
};

struct iphm_state;

struct bucket_handler
{
	size_t value_size;
	uintptr_t (*get_value)(struct iphm_state*, uintptr_t);
	void (*add_value)(struct iphm_state*, uintptr_t, uintptr_t);
};

struct rgbx
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t x;
};

struct iphm_state
{
	uint32_t bits;
	uint32_t count;

	const struct bucket_handler* handler;
	size_t bucket_size;
	union
	{
		void* bucket;
		uint8_t* bucket_u8;
		uint16_t* bucket_u16;
		uint32_t* bucket_u32;
	};

	uintptr_t* seen;

	struct rgbx palette[256];
};

#define SEEN_BYTESIZE 0x20000000

static bool parse_arguments(struct iphm_state*, int, char**);
static bool prepare(struct iphm_state*);
static bool read_address(struct iphm_state*, struct cidr_address*);
static void add_address(struct iphm_state*, const struct cidr_address*);
static void add_address_single(struct iphm_state*, uint32_t);
static uintptr_t bucket_u8_get(struct iphm_state*, uintptr_t);
static void bucket_u8_add(struct iphm_state*, uintptr_t, uintptr_t);
static uintptr_t bucket_u16_get(struct iphm_state*, uintptr_t);
static void bucket_u16_add(struct iphm_state*, uintptr_t, uintptr_t);
static uintptr_t bucket_u32_get(struct iphm_state*, uintptr_t);
static void bucket_u32_add(struct iphm_state*, uintptr_t, uintptr_t);
static void generate_palette(struct iphm_state*);
static void render_image(struct iphm_state*);
static void cleanup(struct iphm_state*);

static const struct bucket_handler bucket_handler_u8 = { sizeof(uint8_t), bucket_u8_get, bucket_u8_add };
static const struct bucket_handler bucket_handler_u16 = { sizeof(uint16_t), bucket_u16_get, bucket_u16_add };
static const struct bucket_handler bucket_handler_u32 = { sizeof(uint32_t), bucket_u32_get, bucket_u32_add };

int main(int argc, char** argv)
{
	__attribute((cleanup(cleanup)))
	struct iphm_state state = {};

	if (!parse_arguments(&state, argc, argv))
		return 1;

	if (!prepare(&state))
		return 2;

	struct cidr_address address;
	while (read_address(&state, &address))
		add_address(&state, &address);

	generate_palette(&state);
	render_image(&state);

	return 0;
}

static bool parse_arguments(struct iphm_state* state, int argc, char** argv)
{
	state->bits = 18;

	if (argc <= 1)
		return true;

	char* bstr = argv[1];
	if (*bstr != '/')
		return false;

	char* endptr;
	unsigned long int value = strtoul(bstr + 1, &endptr, 10);

	if (*endptr || value < 1 || value > 32)
		return false;

	state->bits = value;
	return true;
}

static bool prepare(struct iphm_state* state)
{
	if (state->bits > 24)
		state->handler = &bucket_handler_u8;
	else if (state->bits > 16)
		state->handler = &bucket_handler_u16;
	else
		state->handler = &bucket_handler_u32;

	state->bucket_size = (1UL << state->bits) * state->handler->value_size;
	state->bucket = mmap(NULL, state->bucket_size, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
	if (state->bucket == MAP_FAILED)
		return false;

	state->seen = mmap(NULL, SEEN_BYTESIZE, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
	if (state->seen == MAP_FAILED)
		return false;

	return true;
}

static bool read_address(struct iphm_state* state, struct cidr_address* address)
{
	memset(address, 0, sizeof(struct cidr_address));

	int c;

consume_whitespace:
	c = getchar();
	switch (c)
	{
		case '\t':
		case ' ':
			goto consume_whitespace;
		case EOF:
			return false;
	}

	if (c == '#')
	{
consume_comment:
		c = getchar();
		switch (c)
		{
			case '\n':
				return true;
			case EOF:
				return false;
			default:
				goto consume_comment;
		}
	}

	ungetc(c, stdin);

	uint32_t values[5] = {};
	for (uint32_t cval = 0; cval < 5; ++cval)
	{
consume_value:
		c = getchar();
		switch (c)
		{
			case '\n':
			case EOF:
				if (cval == 3)
				{
					values[4] = 32u;
					goto checkup;
				}
				else if (cval == 4)
					goto checkup;
				else
					return c != EOF;
			case '/':
				if (cval == 3)
					continue;
				else
					return true;
			case '.':
				if (cval < 4)
					continue;
				else
					return true;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				values[cval] = (values[cval] * 10u) + (c - '0');
				goto consume_value;
			default:
				return true;
		}
	}

checkup:
	if (values[0] > 255u ||
		values[1] > 255u ||
		values[2] > 255u ||
		values[3] > 255u ||
		values[4] > 32u)
		return true;

	address->address = (values[0] << 24) | (values[1] << 16) | (values[2] << 8) | values[3];
	address->count = values[4];
	return true;
}

static void add_address(struct iphm_state* state, const struct cidr_address* address)
{
	if (!address->count)
		return;

	if (address->count >= 32u)
	{
		add_address_single(state, address->address);
		return;
	}

	uint32_t count = 1u << (32u - address->count);
	uint32_t base = address->address & ~(count - 1u);

	for (uint32_t i = 0; i < count; ++i)
		add_address_single(state, base | i);
}

static void add_address_single(struct iphm_state* state, uint32_t address)
{
	const uintptr_t ptr_one = 1u;
	const size_t ptr_bits = sizeof(uintptr_t) * 8u;

	uint32_t seen_index = address / ptr_bits;
	uint32_t seen_offset = address % ptr_bits;
	uintptr_t seen_mask = ptr_one << seen_offset;

	uint32_t bucket_index = address >> (32u - state->bits);
	uintptr_t bucket_addend = ((state->seen[seen_index] & seen_mask) >> seen_offset) ^ ptr_one;
	state->seen[seen_index] |= seen_mask;

	state->handler->add_value(state, bucket_index, bucket_addend);
}

static uintptr_t bucket_u8_get(struct iphm_state* state, uintptr_t index)
{
	return state->bucket_u8[index];
}

static void bucket_u8_add(struct iphm_state* state, uintptr_t index, uintptr_t value)
{
	state->bucket_u8[index] += value;
}

static uintptr_t bucket_u16_get(struct iphm_state* state, uintptr_t index)
{
	return state->bucket_u16[index];
}

static void bucket_u16_add(struct iphm_state* state, uintptr_t index, uintptr_t value)
{
	state->bucket_u16[index] += value;
}

static uintptr_t bucket_u32_get(struct iphm_state* state, uintptr_t index)
{
	return state->bucket_u32[index];
}

static void bucket_u32_add(struct iphm_state* state, uintptr_t index, uintptr_t value)
{
	state->bucket_u32[index] += value;
}

static void generate_palette(struct iphm_state* state)
{
	for (uint32_t i = 0u; i < 256u; ++i)
	{
		struct rgbx* p = state->palette + i;

		p->r = i >= 128u ? 255u : i * 2u;

		uint32_t j = i ^ 255u;
		p->g = j >= 128u ? 255u : j * 2u;

		p->b = 0u;
		p->x = 255u;
	}
}

static void render_image(struct iphm_state* state)
{
	uint32_t width = 1u << ((state->bits / 2u) + (state->bits & 1u));
	uint32_t height = 1u << (state->bits / 2u);

	double max = 1u << (32u - state->bits);

	printf("P6\n%u %u 255\n", width, height);

	uint32_t x, y;
	uint32_t* dispatch[2] = { &x, &y };

	for (y = 0u; y < height; ++y)
	{
		for (x = 0u; x < width; ++x)
		{
			uintptr_t index = 0u;
			for (uint32_t i = 0u; i < state->bits; ++i)
			{
				uint32_t* value = dispatch[i & 1u];
				uint32_t bit = (*value >> (i / 2u)) & 1u;
				index |= bit << i;
			}

			uintptr_t raw_value = state->handler->get_value(state, index);

			double scaled = raw_value / max;

			uint32_t value = (uint32_t)round(scaled * 255.0);

			putchar(state->palette[value].r);
			putchar(state->palette[value].g);
			putchar(state->palette[value].b);
		}
	}
}

static void cleanup(struct iphm_state* state)
{
	if (state->seen && state->seen != MAP_FAILED)
		munmap(state->seen, SEEN_BYTESIZE);

	if (state->bucket && state->bucket != MAP_FAILED)
		munmap(state->bucket, state->bucket_size);
}
