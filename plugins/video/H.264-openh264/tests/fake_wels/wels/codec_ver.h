#ifndef STUB_WELS_CODEC_VER_H
#define STUB_WELS_CODEC_VER_H

struct OpenH264Version {
  unsigned int uMajor;
  unsigned int uMinor;
  unsigned int uRevision;
  unsigned int uReserved;
};

void WelsGetCodecVersionEx(OpenH264Version*);

#endif
