
#include <MppDecoder/Decoder.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    mpp_err("invalid input\n");
    return -1;
  }
  char *input = argv[1];
  MppCodingType type = MPP_VIDEO_CodingAVC;

  mpp_log("mpi_dec_test start\n");
  MppDecoder::Decoder decoder;
  decoder.init(input, "./111.yuv", type, 2560, 1440, 20);
  decoder.decode();
  decoder.deinit();
  return 0;
}
