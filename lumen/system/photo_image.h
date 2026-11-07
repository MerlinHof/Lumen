#ifndef LUMEN_PHOTO_IMAGE_H
#define LUMEN_PHOTO_IMAGE_H
/* Canonical photo files are losslessly stored in either orientation. */
int photo_image_read(const char *path, unsigned char canvas[600*800]);
int photo_image_rotate(const char *path);
#endif
