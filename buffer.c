/// @file buffer.c
/// @brief Dynamic buffer management module
/// @author Kenny Huang

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "buffer.h"

#define BUFFER_INCREASE 512

/// @cond
int vpush_buffer(Buffer *b, const char *fmt, va_list ap);
/// @endcond

void BufferDataDestroy(Buffer *b)
{
  if (b && b->data) {
    free(b->data);
    b->data = NULL;
    b->buf_len = 0;
    b->data_len = 0;
  }
}

void BufferInit(Buffer *b)
{
  if (b) {
    b->buf_len = 0;
    b->data_len = 0;
    b->data = NULL;
  }
}

void BufferTrim(Buffer *b, int len)
{
  if (b) {
    if (b->data && b->buf_len >= len && b->data_len >= len) {
      b->data_len -= len;
      b->data[b->data_len] = '\0';
    }
  }
}


void BufferReset(Buffer *b)
{
  if (b) {
    b->data_len = 0;

    if (b->data) {
      b->data[0] = '\0';
    }
  }
}


int vpush_buffer(Buffer *b, const char *fmt, va_list ap)
{
  char *np;
  va_list copy;

  for (;;) {
    int n;
    va_copy(copy, ap);

    if (b->data == NULL) {
      char data[1];
      b->data_len = 0;
      b->buf_len = 0;
      n = vsnprintf(data, sizeof(data), fmt, copy);
      va_end(copy);
    } else {
      n = vsnprintf(b->data + b->data_len, b->buf_len - b->data_len, fmt, copy);
      va_end(copy);
    }

    if (n > -1 && n < b->buf_len - b->data_len) {
      b->data_len += n;
      return 0;
    } else if (b->data) {
      b->data[b->data_len] = '\0';
    }

    // Increase buffer while not enough
    if (n > -1) {
      np = realloc(b->data, b->buf_len + n + 2);
    } else {
      np = realloc(b->data, b->buf_len + BUFFER_INCREASE);
    }

    if (np) {
      b->data = np;

      if (n > -1) {
        b->buf_len += (n + 2);
      } else {
        b->buf_len += BUFFER_INCREASE;
      }
    } else {
      LOG(LOG_ERROR, "Out of memory!");
      return -1;
    }
  }
}

int BufferPush(Buffer *b, const char *fmt, ...)
{
  va_list ap;
  int res;
  va_start(ap, fmt);
  res = vpush_buffer(b, fmt, ap);
  va_end(ap);
  return res;
}

int BufferBinPush(Buffer *b, const void *data, int len)
{
  if (len <= 0) {
    return 0;
  }

  if (b->data == NULL) {
    b->data = malloc(len + 1);

    if (b->data == NULL) {
      LOG(LOG_ERROR, "Out of memory!");
      return -1;
    }

    b->buf_len = len + 1;
    b->data_len = 0;
    b->data[0] = '\0';
  } else if (b->buf_len < b->data_len + len + 1) {
    char *tmp;
    tmp = realloc(b->data, b->data_len + len + 1);

    if (tmp == NULL) {
      LOG(LOG_ERROR, "Out of memory!");
      return -1;
    }

    b->data = tmp;
    b->buf_len = b->data_len + len + 1;
  }

  memmove(b->data + b->data_len, data, len);
  b->data_len += len;
  b->data[b->data_len] = '\0';
  return 0;
}
