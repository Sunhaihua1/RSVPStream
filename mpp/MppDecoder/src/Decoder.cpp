#include <MppDecoder/Decoder.h>

namespace MppDecoder {
Decoder::Decoder()
    : mFps(0), mEos(0), mID(0), mFrmCnt(0), mFrmRate(0.0), mFin(NULL),
      mFout(NULL), mPktBuf(NULL), mCtx(NULL), mApi(NULL), mPkt(NULL),
      mFrmGrp(NULL), mMaxFrmNum(-1) {}

Decoder::~Decoder() {
  if (mPkt) {
    mpp_packet_deinit(&mPkt);
    mPkt = NULL;
  }
  if (mAvpacket) {
    mpp_packet_deinit(&mAvpacket);
    mAvpacket = NULL;
  }
  if (mCtx) {
    mpp_destroy(mCtx);
    mCtx = NULL;
  }

  if (mPktBuf) {
    delete[] mPktBuf;
    mPktBuf = NULL;
  }

  if (mBuffer) {
    mpp_buffer_put(mBuffer);
    mBuffer = NULL;
  }

  if (mFrmGrp) {
    mpp_buffer_group_put(mFrmGrp);
    mFrmGrp = NULL;
  }

  if (mFin) {
    fclose(mFin);
    mFin = NULL;
  }

  if (mFout) {
    fclose(mFout);
    mFout = NULL;
  }
}

int Decoder::init(const char *file_input, const char *file_output,
                  MppCodingType type, int src_w, int src_h, int mMaxFrameNum) {
  int ret = 0;
  this->mMaxFrmNum = mMaxFrameNum;
  int x, y, i;
  RK_U32 need_split = 1;
  MpiCmd mpi_cmd = MPP_CMD_BASE;
  MppParam param = NULL;
  srcW = src_w;
  srcH = src_h;

  mFin = fopen(file_input, "rb");
  if (!mFin) {
    mpp_log("failed to open input file %s.\n", file_input);
    return -1;
  }

  mFout = fopen(file_output, "wb+");
  if (!mFout) {
    mpp_log("failed to open output file %s.\n", file_output);
    return -2;
  }

  mPktBuf = new uint8_t[PKT_SIZE];
  if (!mPktBuf) {
    mpp_log("failed to malloc mPktBuf.\n");
    return -3;
  }

  ret = mpp_packet_init(&mPkt, mPktBuf, PKT_SIZE);
  if (ret) {
    mpp_log("failed to exec mpp_packet_init ret %d", ret);
    return -4;
  }

  ret = mpp_create(&mCtx, &mApi);
  if (ret != MPP_OK) {
    mpp_log("failed to exec mpp_create ret %d", ret);
    return -5;
  }

  mpi_cmd = MPP_DEC_SET_PARSER_SPLIT_MODE;
  param = &need_split;
  ret = mApi->control(mCtx, mpi_cmd, param);
  if (ret != MPP_OK) {
    mpp_log("failed to control MPP_DEC_SET_PARSER_SPLIT_MODE.\n");
    return -6;
  }

  ret = mpp_init(mCtx, MPP_CTX_DEC, type);
  if (ret != MPP_OK) {
    mpp_log("failed to exec mpp_init.\n");
    return -7;
  }

  return 0;
}

static RK_S64 get_time() {
  struct timeval tv_date;
  gettimeofday(&tv_date, NULL);
  return (RK_S64)tv_date.tv_sec * 1000000 + (RK_S64)tv_date.tv_usec;
}

int Decoder::dump_mpp_frame_to_file(MppFrame frame, FILE *fp) {
  RK_U32 width = 0;
  RK_U32 height = 0;
  RK_U32 h_stride = 0;
  RK_U32 v_stride = 0;
  MppFrameFormat fmt = MPP_FMT_YUV420SP;
  MppBuffer buffer = NULL;
  RK_U8 *base = NULL;

  if (!fp || !frame) {
    mpp_log("failed to dump frame to file fp %p frame %p.\n", fp, frame);
    return -1;
  }
  width = mpp_frame_get_width(frame);
  height = mpp_frame_get_height(frame);
  h_stride = mpp_frame_get_hor_stride(frame);
  v_stride = mpp_frame_get_ver_stride(frame);
  fmt = mpp_frame_get_fmt(frame);
  buffer = mpp_frame_get_buffer(frame);
  mpp_log("dump frame w %d h %d h_stride %d v_stride %d fmt %d buffer %p.\n",
          width, height, h_stride, v_stride, fmt, buffer);
  if (!buffer)
    return -2;

  base = (RK_U8 *)mpp_buffer_get_ptr(buffer);
  {
    RK_U32 i;
    RK_U8 *base_y = base;
    RK_U8 *base_u = base + h_stride * v_stride;
    RK_U8 *base_v = base_u + h_stride * v_stride / 4;

    for (i = 0; i < height; i++, base_y += h_stride)
      fwrite(base_y, 1, width, fp);
    for (i = 0; i < height / 2; i++, base_u += h_stride / 2)
      fwrite(base_u, 1, width / 2, fp);
    for (i = 0; i < height / 2; i++, base_v += h_stride / 2)
      fwrite(base_v, 1, width / 2, fp);
  }

  return 0;
}

int Decoder::decode_one_pkt(uint8_t *buf, int size, MppFrame *srcFrm) {
  int ret = 0;
  RK_U32 pkt_done = 0;
  mpp_log("init packet");
  mpp_packet_init(&mPkt, buf, size);
  mpp_packet_write(mPkt, 0, buf, size);
  mpp_packet_set_pos(mPkt, buf);
  mpp_packet_set_length(mPkt, size);

  if (mEos)
    mpp_packet_set_eos(mPkt);

  do {
    RK_S32 times = 5;

    if (!pkt_done) {
      ret = mApi->decode_put_packet(mCtx, mPkt);
      if (ret == MPP_OK)
        pkt_done = 1;
    }

    do {
      RK_S32 get_frm = 0;
      RK_U32 frm_eos = 0;

    try_again:
      ret = mApi->decode_get_frame(mCtx, srcFrm);
      if (ret == MPP_ERR_TIMEOUT) {
        if (times > 0) {
          times--;
          usleep(2000);
          goto try_again;
        }
        mpp_log("decode_get_frame failed too much time.\n");
      }

      if (ret != MPP_OK) {
        mpp_log("decode_get_frame failed ret %d.\n", ret);
        break;
      }

      if (*srcFrm) {
        if (mpp_frame_get_info_change(*srcFrm)) {
          RK_U32 width = mpp_frame_get_width(*srcFrm);
          RK_U32 height = mpp_frame_get_height(*srcFrm);
          RK_U32 hor_stride = mpp_frame_get_hor_stride(*srcFrm);
          RK_U32 ver_stride = mpp_frame_get_ver_stride(*srcFrm);

          mpp_log("decode_get_frame get info changed found.\n");
          mpp_log("decoder require buffer w:h [%d:%d] stride [%d:%d]", width,
                  height, hor_stride, ver_stride);

          ret = mpp_buffer_group_get_internal(&mFrmGrp, MPP_BUFFER_TYPE_DRM);
          if (ret) {
            mpp_log("get mpp buffer group failed ret %d.\n", ret);
            break;
          }

          mApi->control(mCtx, MPP_DEC_SET_EXT_BUF_GROUP, mFrmGrp);
          mApi->control(mCtx, MPP_DEC_SET_INFO_CHANGE_READY, NULL);
        } else {
          RK_U32 err_info =
              mpp_frame_get_errinfo(*srcFrm) | mpp_frame_get_discard(*srcFrm);
          if (err_info) {
            mpp_log("decoder_get_frame get err info:%d discard:%d.\n",
                    mpp_frame_get_errinfo(*srcFrm),
                    mpp_frame_get_discard(*srcFrm));
          }

          if (mFout && !err_info) {
            /*
             * note that write file will leads to IO block
             * so if you want to test frame rate,don't wirte
             * it.
             */
            mFrmCnt++;
            cv::Mat rgbImg;
            int width = mpp_frame_get_width(*srcFrm);
            YUV2Mat(*srcFrm, rgbImg);
            cv::imwrite("./" + std::to_string(mFrmCnt) + ".jpg", rgbImg);
            dump_mpp_frame_to_file(*srcFrm, mFout);
          }
        }
        frm_eos = mpp_frame_get_eos(*srcFrm);
        mpp_frame_deinit(srcFrm);
        *srcFrm = NULL;
        get_frm = 1;
      }

      if (mEos && pkt_done && !frm_eos) {
        usleep(10000);
        continue;
      }

      if (frm_eos) {
        mpp_log("found last frame.\n");
        break;
      }
      if (mMaxFrmNum >= 0 && mFrmCnt >= mMaxFrmNum) {
        mEos = 1;
        break;
      }
      if (get_frm)
        continue;
      break;
    } while (1);

    if (pkt_done)
      break;
    /*
     * why sleep here?
     * mpp has a internal input pakcet list,if it is full, wait here 3ms to
     * wait internal list isn't full.
     */
    usleep(3000);
  } while (1);

  return 0;
}

int Decoder::decode() {
  MppFrame srcFrm;
  double timeDiff;
  int ret = 0;
  int hor_stride = DECODER_ALIGN(srcW, 16);
  int ver_srride = DECODER_ALIGN(srcH, 16);

  mpp_buffer_get(mFrmGrp, &mBuffer, hor_stride * ver_srride * 3 / 2);
  auto start = std::chrono::high_resolution_clock::now();
  // mTimeS = get_time();
  while (!mEos) {
    size_t read_size = fread(mPktBuf, 1, PKT_SIZE, mFin);
    if (read_size != PKT_SIZE || feof(mFin)) {
      mpp_log("found last packet.\n");
      mEos = 1;
    }

    ret = decode_one_pkt(mPktBuf, read_size, &srcFrm);
    if (ret < 0) {
      mpp_log("failed to exec decode.\n");
      return -2;
    }
  }
  auto end = std::chrono::high_resolution_clock::now();
  // mTimeE = get_time();
  // timeDiff = double(mTimeE - mTimeS)/1000;
  std::chrono::duration<double, std::milli> elapsed = end - start;
  mFrmRate = (mFrmCnt * 1000) / elapsed.count();
  mpp_log("decode frames %d using %.2fms frm rate:%.2f.\n", mFrmCnt,
          elapsed.count(), mFrmRate);

  return 0;
}
int Decoder::decode_ffmpeg() {
  MppFrame srcFrm;
  double timeDiff;
  int ret = 0;
  int hor_stride = DECODER_ALIGN(srcW, 16);
  int ver_srride = DECODER_ALIGN(srcH, 16);
  AVFormatContext *pFormatCtx = NULL;
  AVDictionary *options = NULL;
  AVPacket *av_packet = NULL;
  avformat_network_init();
  av_dict_set(&options, "buffer_size", "1024000",
              0); // 设置缓存大小,1080p可将值跳到最大
  av_dict_set(&options, "rtsp_transport", "udp", 0); // 以udp的方式打开,
  av_dict_set(&options, "stimeout", "5000000",
              0); // 设置超时断开链接时间，单位us
  av_dict_set(&options, "max_delay", "500000", 0); // 设置最大时延
  // 打开网络流或文件流
  const char *filepath = "rtsp://10.42.0.1:8554/live";
  if (avformat_open_input(&pFormatCtx, filepath, NULL, &options) != 0) {
    printf("Couldn't open input stream.\n");
    return 0;
  }

  // 获取视频文件信息
  if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
    printf("Couldn't find stream information.\n");
    return 0;
  }

  // 查找码流中是否有视频流
  int videoindex = -1;
  for (int i = 0; i < pFormatCtx->nb_streams; i++)
    if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      videoindex = i;
      break;
    }
  if (videoindex == -1) {
    printf("Didn't find a video stream.\n");
    return 0;
  }

  int count = 0;
  // 获取图像宽高
  int srcWidth = pFormatCtx->streams[videoindex]->codecpar->width;
  int srcHeight = pFormatCtx->streams[videoindex]->codecpar->height;
  printf("srcWidth:%d,srcHeight:%d\n", srcWidth, srcHeight);

  av_packet = (AVPacket *)av_malloc(
      sizeof(AVPacket)); // 申请空间，存放的每一帧数据 （h264、h265）
  av_new_packet(av_packet, srcWidth * srcHeight);
  auto start = std::chrono::high_resolution_clock::now();
  // //mTimeS = get_time();
  while (!mEos) {
    if (av_read_frame(pFormatCtx, av_packet) >= 0) {
      if (av_packet->stream_index == videoindex) {
        mpp_log("--------------\ndata size is: %d\n-------------",
                av_packet->size);
        ret = decode_one_pkt(av_packet->data, av_packet->size, &srcFrm);
        if (ret < 0) {
          mpp_log("failed to exec decode.\n");
          return -2;
        }
      }
      if (av_packet != NULL)
        av_packet_unref(av_packet);
    }
  }
  auto end = std::chrono::high_resolution_clock::now();
  // mTimeE = get_time();
  // timeDiff = double(mTimeE - mTimeS)/1000;
  std::chrono::duration<double, std::milli> elapsed = end - start;
  mFrmRate = (mFrmCnt * 1000) / elapsed.count();
  mpp_log("decode frames %d using %.2fms frm rate:%.2f.\n", mFrmCnt,
          elapsed.count(), mFrmRate);
  av_free(av_packet);
  avformat_close_input(&pFormatCtx);

  return 0;
}
// int Decoder::decode_one_avpacket(AVPacket* av_packet, MppFrame *srcFrm) {
//     int ret = 0;
//     RK_U32 pkt_done = 0;

//     mpp_log("init packet");
//     ret = mpp_packet_init(&mAvpacket, av_packet->data, av_packet->size);
//     mpp_packet_set_data(mAvpacket, av_packet->data);
//     mpp_packet_set_size(mAvpacket, av_packet->size);
//     mpp_packet_set_pos(mAvpacket, av_packet->data);
//     mpp_packet_set_length(mAvpacket, av_packet->size);
//     mpp_packet_set_pts(mAvpacket, av_packet->pts);

//     do {
//         RK_S32 times = 5;

//         if (!pkt_done) {
//             ret = mApi->decode_put_packet(mCtx, mAvpacket);
//             if (ret == MPP_OK)
//             	mpp_log("decode_put_packet success");
//                 pkt_done = 1;
//         }

//         do {
//             RK_S32 get_frm = 0;
//             RK_U32 frm_eos = 0;

//             try_again:
//                 ret = mApi->decode_get_frame(mCtx, srcFrm);
//                 if (ret == MPP_ERR_TIMEOUT) {
//                     if (times > 0) {
//                         times--;
//                         usleep(2000);
//                         goto try_again;
//                     }
//                     mpp_log("decode_get_frame failed too much time.\n");
//                 }

//                 if (ret != MPP_OK) {
//                     mpp_log("decode_get_frame failed ret %d.\n", ret);
//                     break;
//                 }

//                 if (*srcFrm) {
//                     mpp_log("decode_get_frame success");
//                     if (mpp_frame_get_info_change(*srcFrm)) {
//                         RK_U32 width = mpp_frame_get_width(*srcFrm);
//                         RK_U32 height = mpp_frame_get_height(*srcFrm);
//                         RK_U32 hor_stride =
//                         mpp_frame_get_hor_stride(*srcFrm); RK_U32 ver_stride
//                         = mpp_frame_get_ver_stride(*srcFrm);

//                         mpp_log("decode_get_frame get info changed
//                         found.\n"); mpp_log("decoder require buffer w:h
//                         [%d:%d] stride [%d:%d]",
//                                 width, height, hor_stride, ver_stride);

//                         ret = mpp_buffer_group_get_internal(&mFrmGrp,
//                         MPP_BUFFER_TYPE_DRM); if (ret) {
//                             mpp_log("get mpp buffer group failed ret %d.\n",
//                             ret); break;
//                         }

//                         mApi->control(mCtx, MPP_DEC_SET_EXT_BUF_GROUP,
//                         mFrmGrp); mApi->control(mCtx,
//                         MPP_DEC_SET_INFO_CHANGE_READY, NULL);
//                     } else {
//                         RK_U32 err_info = mpp_frame_get_errinfo(*srcFrm) |
//                         mpp_frame_get_discard(*srcFrm); if (err_info)
//                         {
//                             mpp_log("decoder_get_frame get err info:%d
//                             discard:%d.\n", mpp_frame_get_errinfo(*srcFrm),
//                             mpp_frame_get_discard(*srcFrm));
//                         }

//                         if (mFout && !err_info) {
//                             mFrmCnt++;
//                             /*
//                                 * note that write file will leads to IO block
//                                 * so if you want to test frame rate,don't
//                                 wirte
//                                 * it.
//                                 */
//                             cv::Mat rgbImg;
//                             int width = mpp_frame_get_width(*srcFrm);
//                             YUV2Mat(*srcFrm, rgbImg);
//                             cv::imwrite("./" + std::to_string(mFrmCnt) +
//                             ".jpg", rgbImg); dump_mpp_frame_to_file(*srcFrm,
//                             mFout);
//                         }
//                     }
//                     frm_eos = mpp_frame_get_eos(*srcFrm);
//                     mpp_frame_deinit(srcFrm);
//                     *srcFrm = NULL;
//                     get_frm = 1;
//                 }

//                 if (mEos && pkt_done && !frm_eos) {
//                     usleep(10000);
//                     continue;
//                 }

//                 if (frm_eos) {
//                     mpp_log("found last frame.\n");
//                     break;
//                 }
//                 if (mMaxFrmNum >= 0 && mFrmCnt >= mMaxFrmNum) {
//                     mEos = 1;
//                     break;
//                 }
//                 if (get_frm)
//                     continue;
//                 break;
//         } while (1);

//         if (pkt_done)
//             break;
//         /*
//         * why sleep here?
//         * mpp has a internal input pakcet list,if it is full, wait here 3ms
//         to
//         * wait internal list isn't full.
//         */
//         usleep(3000);
//     } while (1);
//     return 0;
// }
void Decoder::YUV2Mat(MppFrame frame, cv::Mat &rgbImg) {
  RK_U32 width = 0;
  RK_U32 height = 0;
  RK_U32 h_stride = 0;
  RK_U32 v_stride = 0;
  MppBuffer buffer = NULL;
  RK_U8 *base = NULL;

  width = mpp_frame_get_width(frame);
  height = mpp_frame_get_height(frame);
  h_stride = mpp_frame_get_hor_stride(frame);
  v_stride = mpp_frame_get_ver_stride(frame);
  buffer = mpp_frame_get_buffer(frame);

  base = (RK_U8 *)mpp_buffer_get_ptr(buffer);
  RK_U32 buf_size = mpp_frame_get_buf_size(frame);
  size_t base_length = mpp_buffer_get_size(buffer);
  RK_U32 i;
  RK_U8 *base_y = base;
  RK_U8 *base_c = base + h_stride * v_stride;

  cv::Mat yuvImg;
  yuvImg.create(height * 3 / 2, width, CV_8UC1);

  // 转为YUV420sp格式
  int idx = 0;
  for (i = 0; i < height; i++, base_y += h_stride) {
    //        fwrite(base_y, 1, width, fp);
    memcpy(yuvImg.data + idx, base_y, width);
    idx += width;
  }
  for (i = 0; i < height / 2; i++, base_c += h_stride) {
    //        fwrite(base_c, 1, width, fp);
    memcpy(yuvImg.data + idx, base_c, width);
    idx += width;
  }
  // 这里的转码需要转为RGB 3通道， RGBA四通道则不能检测成功
  cv::cvtColor(yuvImg, rgbImg, cv::COLOR_YUV420sp2RGB);
}

int Decoder::deinit() {
  int ret = 0;

  ret = mApi->reset(mCtx);
  if (ret != MPP_OK) {
    mpp_log("failed to exec mApi->reset.\n");
    return -1;
  }
  return ret;
}
} // namespace MppDecoder
