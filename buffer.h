/// @file buffer.h
/// @brief Dynamic buffer management module header
/// @author Kenny Huang

#ifndef __HEADER_BUFFER__
#define __HEADER_BUFFER__

/// The dynamic buffer to store a unknown length data
typedef struct Buffer {
  int buf_len;    ///< Current buffer length
  int data_len;   ///< Current data length
  char *data;     ///< The data buffer
} Buffer;

/// @defgroup buffer Dynamic message buffer
/// @{

/// @brief Free a buffer data, but the buffer pointer still exists,
///        user has to call free() to reloeas buffer pointer
/// @param[in,out] b The buffer
void BufferDataDestroy(Buffer *b);

/// @brief Initiate a buffer
/// @param[in,out] b The buffer
void BufferInit(Buffer *b);

/// @brief Delete a certain length of data from the buffer tail
/// @param[in,out] b The buffer
/// @param[in] len The length of data to be delete
void BufferTrim(Buffer *b, int len);

/// @brief Reset the buffer content, not free
/// @param[in,out] b The buffer
void BufferReset(Buffer *b);

/// @brief Push formatted string data into the buffer
/// @param[in,out] b A buffer struct pointer
/// @param[in] fmt The format string of the data argument, just as the same as
///                         printf()
/// @param[in] ... The additional arguments following **fmt** are formatted and
///                       inserted in the resulting string replacing their respective 
///                       specifiers.
/// @return 0: On success
/// @return -1: On error
int BufferPush(Buffer *b, const char *fmt, ...);

/// @brief Push binary data into the buffer
/// @param[in,out] b A buffer struct pointer
/// @param[in] data Binary data
/// @param[in] len Data length in byte
/// @return 0: On success
/// @return -1: On error
int BufferBinPush(Buffer *b, const void *data, int len);

/// @}

#endif //__HEADER_BUFFER__
