/*
 * Copyright (c) 2014 Hugh Bailey <obs.jim@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "darray.h"
#include "array-serializer.h"

#include <string.h>

static size_t array_output_write(void *param, const void *data, size_t size)
{
	struct array_output_data *output = param;
	size_t old_size = output->bytes.num;
	size_t end_pos;

	if (!size)
		return 0;

	if (SIZE_MAX - output->pos < size)
		return 0;

	end_pos = output->pos + size;
	if (end_pos > output->bytes.num) {
		da_resize(output->bytes, end_pos);

		if (output->pos > old_size)
			memset(output->bytes.array + old_size, 0,
			       output->pos - old_size);
	}

	memcpy(output->bytes.array + output->pos, data, size);
	output->pos = end_pos;
	return size;
}

static int64_t array_output_seek(void *param, int64_t offset,
				 enum serialize_seek_type seek_type)
{
	struct array_output_data *data = param;
	int64_t base = 0;
	int64_t pos;

	switch (seek_type) {
	case SERIALIZE_SEEK_START:
		base = 0;
		break;
	case SERIALIZE_SEEK_CURRENT:
		base = (int64_t)data->pos;
		break;
	case SERIALIZE_SEEK_END:
		base = (int64_t)data->bytes.num;
		break;
	}

	if (offset > INT64_MAX - base)
		return -1;

	pos = base + offset;
	if (pos < 0)
		return -1;

	data->pos = (size_t)pos;
	return pos;
}

static int64_t array_output_get_pos(void *param)
{
	struct array_output_data *data = param;
	return (int64_t)data->pos;
}

void array_output_serializer_init(struct serializer *s,
				  struct array_output_data *data)
{
	memset(s, 0, sizeof(struct serializer));
	memset(data, 0, sizeof(struct array_output_data));
	s->data = data;
	s->write = array_output_write;
	s->seek = array_output_seek;
	s->get_pos = array_output_get_pos;
}

void array_output_serializer_reset(struct array_output_data *data)
{
	da_resize(data->bytes, 0);
	data->pos = 0;
}

void array_output_serializer_free(struct array_output_data *data)
{
	da_free(data->bytes);
}
