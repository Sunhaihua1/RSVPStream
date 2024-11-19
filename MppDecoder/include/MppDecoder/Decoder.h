
#pragma once
#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif
#include <libavformat/avformat.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

#include "mpp_buffer.h"
#include "mpp_common.h"
#include "mpp_err.h"
#include "mpp_frame.h"
#include "mpp_log.h"
#include "mpp_meta.h"
#include "mpp_packet.h"
#include "mpp_task.h"
#include "rk_mpi.h"
#include "rk_type.h"
#include "vpu.h"
#include "vpu_api.h"

#include "rk_mpi_cmd.h"
#ifdef __cplusplus
}
#endif

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#define PKT_SIZE (SZ_4K)
#define DECODER_ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))

namespace MppDecoder {
class Decoder {
public:
  // 构造函数
  Decoder();
  // 析构函数,释放所占用的Mpp资源，防止内存泄露
  ~Decoder();
  int init(const char *file_input, const char *file_output, MppCodingType type,
           int src_w, int src_h, int mMaxFrameNum);
  // deinit
  int deinit();
  // 阻塞式解码，会一直循环判断，调用decode_one_pkt
  int decode_one_pkt(uint8_t *buf, int size, MppFrame *srcFrm);
  // 根据码流文件、输出文件、码流类型、视频帧宽高、最大解析视频帧的数量（负数表示解析完当前文件）初始化
  // 一个Decoder
  int decode();
  int decode_ffmpeg();
  int decode_one_avpacket(AVPacket *av_packet, MppFrame *srcFrm);

  // 将视频帧写入文件
  int dump_mpp_frame_to_file(MppFrame frame, FILE *fp);
  // 获取视频帧率
  double get_frm_rate() { return mFrmRate; }
  // 将视频帧转换为cv::Mat
  void YUV2Mat(MppFrame frame, cv::Mat &rgbImg);

private:
  int mFps;
  int mEos;
  int mID;
  int mFrmCnt;
  int srcW;
  int srcH;
  int mMaxFrmNum;
  double mFrmRate;

  FILE *mFin;
  FILE *mFout;
  uint8_t *mPktBuf;

  MppCtx mCtx;
  MppApi *mApi;
  MppPacket mPkt;
  MppBuffer mBuffer;
  MppBufferGroup mFrmGrp;
  MppPacket mAvpacket = NULL;
};
} // namespace MppDecoder
