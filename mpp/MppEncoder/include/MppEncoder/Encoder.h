#pragma once

#include <string.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "mpp_common.h"
#include "mpp_env.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include "mpp_time.h"
#include "rk_mpi.h"
#include "utils.h"
#ifdef __cplusplus
}
#endif

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>

namespace MppEncoder {
class Encoder {
public:
  Encoder();
  ~Encoder();
  int init(int s_width, int s_height, MppFrameFormat fmt, MppCodingType type,
           int frame_num);

  int deinit();
  MPP_RET encoder_ctx_setup();
  MPP_RET read_yuv_buffer(RK_U8 *buf, cv::Mat &yuvImg, RK_U32 width,
                          RK_U32 height);
  MPP_RET encode_YUV(cv::Mat yuvImg, uint8_t *&H264_buf, size_t &length);
  MPP_RET encode_Head(int width, int height, uint8_t *&SPS_buf,
                      size_t &SPS_length);
  int YuvtoH264(int width, int height, cv::Mat yuv_frame,
                unsigned char *(&encode_buf), int &encode_length);

private:
  // global flow control flag
  RK_U32 frm_eos;
  RK_U32 pkt_eos;
  RK_U32 frame_count;
  RK_U64 stream_size;

  // input ang output file
  FILE *mFin;
  FILE *mFout;

  // input and output
  MppBuffer mBuffer;
  MppEncSeiMode sei_mode;

  // base flow context
  MppCtx mCtx;
  MppApi *mApi;
  MppEncPrepCfg prep_cfg;
  MppEncRcCfg rc_cfg;
  MppEncCodecCfg codec_cfg;

  // paramter for resource malloc
  RK_U32 width;
  RK_U32 height;
  RK_U32 hor_stride; // horizontal stride
  RK_U32 ver_stride; // vertical stride
  MppFrameFormat fmt;
  MppCodingType type;
  RK_U32 num_frames;

  // resources
  size_t frame_size;
  // NOTE: packet buffer may overflow
  size_t packet_size;
  uint8_t *SPS_buf;
  size_t SPS_length;
  uint8_t *H264_buf;
  size_t H264_length;
  // rate control runtime parameter
  RK_S32 gop;
  RK_S32 fps;
  RK_S32 bps;
};
} // namespace MppEncoder
