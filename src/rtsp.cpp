#include <MppDecoder/Decoder.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  MppCodingType type = MPP_VIDEO_CodingAVC;
  // MppCodingType type  = MPP_VIDEO_CodingHEVC;

  mpp_log("mpi_dec_test start\n");
  MppDecoder::Decoder decoder;
  decoder.init("/oem/tennis.h264", "./111.yuv", type, 2560, 1440, 200);

  decoder.decode_ffmpeg();
}
