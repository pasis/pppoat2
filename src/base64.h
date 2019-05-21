/* base64.h
 * PPP over Any Transport -- Base64
 *
 * Copyright (C) 2012-2019 Dmitry Podgorny <pasis.ua@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __PPPOAT_BASE64_H__
#define __PPPOAT_BASE64_H__

#include <stdbool.h>	/* bool */
#include <stddef.h>	/* size_t */

size_t pppoat_base64_enc_len(const void *buf, size_t len);
size_t pppoat_base64_dec_len(const char *base64, size_t len);
void pppoat_base64_enc(const void *buf,
		       size_t      len,
		       char       *result,
		       size_t      result_len);
int pppoat_base64_dec(const char *base64,
		      size_t      len,
		      void       *result,
		      size_t      result_len);
int pppoat_base64_enc_new(const void *buf,
			  size_t      len,
			  char      **result);
int pppoat_base64_dec_new(const char     *base64,
			  size_t          len,
			  unsigned char **result,
			  size_t         *result_len);
bool pppoat_base64_is_valid(const char *base64, size_t len);

#endif /* __PPPOAT_BASE64_H__ */
