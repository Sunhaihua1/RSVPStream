#include <MppEncoder/Encoder.h>

namespace MppEncoder {
Encoder::Encoder()
    : width(0), height(0), num_frames(0), mFin(NULL), mFout(NULL), mCtx(NULL),
      mApi(NULL), mBuffer(NULL) {}
Encoder::~Encoder() {
  if (mBuffer) {
    mpp_buffer_put(mBuffer);
    mBuffer = NULL;
  }
  if (mCtx) {
    mpp_destroy(mCtx);
    mCtx = NULL;
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
int Encoder::init(int s_width, int s_height, MppFrameFormat fmt,
                  MppCodingType type, int frame_num) {
  // get paramter from cmd
  this->width = s_width;
  this->height = s_height;
  this->hor_stride = MPP_ALIGN(s_width, 16);
  this->ver_stride = MPP_ALIGN(s_height, 16);
  this->fmt = fmt;
  this->type = type;
  this->num_frames = frame_num;
  this->frame_size = this->hor_stride * this->ver_stride * 3 / 2;
  this->packet_size = this->width * this->height;
  mpp_log("frame_size = %d----------------\n", this->frame_size);
  MPP_RET ret = MPP_OK;

  ret = mpp_buffer_get(NULL, &this->mBuffer, this->frame_size);
  if (ret) {
    mpp_err_f("failed to get buffer for input frame ret %d\n", ret);
    return -1;
  }
  mpp_log("mpi_enc_test encoder test start w %d h %d type %d\n", this->width,
          this->height, this->type);
  // encoder demo
  ret = mpp_create(&this->mCtx, &this->mApi);
  if (ret) {
    mpp_err("mpp_create failed ret %d\n", ret);
    return -2;
  }

  ret = mpp_init(this->mCtx, MPP_CTX_ENC, this->type);
  if (ret) {
    mpp_err("mpp_init failed ret %d\n", ret);
    return -3;
  }
  ret = encoder_ctx_setup();
  if (ret) {
    mpp_err_f("test mpp setup failed ret %d\n", ret);
    return -4;
  }
  if (this->type == MPP_VIDEO_CodingAVC) {
    MppPacket packet = NULL;

    ret = this->mApi->control(mCtx, MPP_ENC_GET_EXTRA_INFO, &packet);
    if (ret) {
      mpp_err("mpi control enc get extra info failed\n");
    }

    // get and write sps/pps for H.264
    if (packet) {
      void *ptr = mpp_packet_get_pos(packet);
      size_t len = mpp_packet_get_length(packet);

      SPS_buf = new unsigned char[len];

      // fwrite(ptr, 1, len, fp_out);
      memcpy(SPS_buf, ptr, len);
      SPS_length = len;

      packet = NULL;
    }
  }
  return 0;
}

MPP_RET Encoder::encoder_ctx_setup() {
  MPP_RET ret;
  MppEncCodecCfg *codec_cfg;
  MppEncPrepCfg *prep_cfg;
  MppEncRcCfg *rc_cfg;

  codec_cfg = &this->codec_cfg;
  prep_cfg = &this->prep_cfg;
  rc_cfg = &this->rc_cfg;

  this->fps = 25;
  this->gop = 50;
  this->bps = this->width * this->height / 5 * this->fps;

  prep_cfg->change = MPP_ENC_PREP_CFG_CHANGE_INPUT |
                     MPP_ENC_PREP_CFG_CHANGE_ROTATION |
                     MPP_ENC_PREP_CFG_CHANGE_FORMAT;
  prep_cfg->width = this->width;
  prep_cfg->height = this->height;
  prep_cfg->hor_stride = this->hor_stride;
  prep_cfg->ver_stride = this->ver_stride;
  prep_cfg->format = this->fmt;
  prep_cfg->rotation = MPP_ENC_ROT_0;
  ret = mApi->control(mCtx, MPP_ENC_SET_PREP_CFG, prep_cfg);
  if (ret) {
    mpp_err("mpi control enc set prep cfg failed ret %d\n", ret);
    goto RET;
  }

  rc_cfg->change = MPP_ENC_RC_CFG_CHANGE_ALL;
  rc_cfg->rc_mode = MPP_ENC_RC_MODE_VBR;
  // rc_cfg->quality = MPP_ENC_RC_QUALITY_MEDIUM;
  rc_cfg->quality = MPP_ENC_RC_QUALITY_CQP;

  if (rc_cfg->rc_mode == MPP_ENC_RC_MODE_CBR) {
    // constant bitrate has very4 small bps range of 1/16 bps
    rc_cfg->bps_target = this->bps;
    rc_cfg->bps_max = this->bps * 17 / 16;
    rc_cfg->bps_min = this->bps * 15 / 16;
  } else if (rc_cfg->rc_mode == MPP_ENC_RC_MODE_VBR) {
    if (rc_cfg->quality == MPP_ENC_RC_QUALITY_CQP) {
      // constant QP does not have bps
      // rc_cfg->bps_target   = -1;
      // rc_cfg->bps_max      = -1;
      // rc_cfg->bps_min      = -1;

      rc_cfg->bps_target = this->bps;
      rc_cfg->bps_max = this->bps * 17 / 16;
      rc_cfg->bps_min = this->bps * 1 / 16;
    } else {
      // variable bitrate has large bps range
      rc_cfg->bps_target = this->bps;
      rc_cfg->bps_max = this->bps * 17 / 16;
      rc_cfg->bps_min = this->bps * 1 / 16;
    }
  }

  // fix input / output frame rate
  rc_cfg->fps_in_flex = 0;
  rc_cfg->fps_in_num = this->fps;
  rc_cfg->fps_in_denorm = 1;
  rc_cfg->fps_out_flex = 0;
  rc_cfg->fps_out_num = this->fps;
  rc_cfg->fps_out_denorm = 1;

  rc_cfg->gop = this->gop;
  rc_cfg->skip_cnt = 0;

  mpp_log("mpi_enc_test bps %d fps %d gop %d\n", rc_cfg->bps_target,
          rc_cfg->fps_out_num, rc_cfg->gop);
  ret = mApi->control(mCtx, MPP_ENC_SET_RC_CFG, rc_cfg);
  if (ret) {
    mpp_err("mpi control enc set rc cfg failed ret %d\n", ret);
    goto RET;
  }

  codec_cfg->coding = this->type;
  codec_cfg->h264.change = MPP_ENC_H264_CFG_CHANGE_PROFILE |
                           MPP_ENC_H264_CFG_CHANGE_ENTROPY |
                           MPP_ENC_H264_CFG_CHANGE_TRANS_8x8;

  // 66  - Baseline profile
  // 77  - Main profile
  // 100 - High profile
  codec_cfg->h264.profile = 77;

  /*
   * H.264 level_idc parameter
   * 10 / 11 / 12 / 13    - qcif@15fps / cif@7.5fps / cif@15fps / cif@30fps
   * 20 / 21 / 22         - cif@30fps / half-D1@@25fps / D1@12.5fps
   * 30 / 31 / 32         - D1@25fps / 720p@30fps / 720p@60fps
   * 40 / 41 / 42         - 1080p@30fps / 1080p@30fps / 1080p@60fps
   * 50 / 51 / 52         - 4K@30fps
   */

  codec_cfg->h264.level = 30;
  codec_cfg->h264.entropy_coding_mode = 1;
  codec_cfg->h264.cabac_init_idc = 0;
  // codec_cfg->h264.qp_min = 0;
  // codec_cfg->h264.qp_max = 50;

  // codec_cfg->h264.transform8x8_mode = 0;

  ret = mApi->control(mCtx, MPP_ENC_SET_CODEC_CFG, codec_cfg);
  if (ret) {
    mpp_err("mpi control enc set codec cfg failed ret %d\n", ret);
    goto RET;
  }

  // optional
  this->sei_mode = MPP_ENC_SEI_MODE_ONE_FRAME;
  ret = mApi->control(mCtx, MPP_ENC_SET_SEI_CFG, &this->sei_mode);
  if (ret) {
    mpp_err("mpi control enc set sei cfg failed ret %d\n", ret);
    goto RET;
  }

RET:
  return ret;
}
MPP_RET Encoder::read_yuv_buffer(RK_U8 *buf, cv::Mat &yuvImg, RK_U32 width,
                                 RK_U32 height) {
  MPP_RET ret = MPP_OK;
  RK_U32 read_size;
  RK_U32 row = 0;
  RK_U8 *buf_y = buf;
  RK_U8 *buf_u = buf_y + width * height;
  RK_U8 *buf_v = buf_u + width * height / 4;

  int yuv_size = width * height * 3 / 2;

  memcpy(buf, yuvImg.data, yuv_size);

err:
  return ret;
}
MPP_RET Encoder::encode_Head(int width, int height, uint8_t *&SPS_buf,
                             size_t &SPS_length) {
  MPP_RET ret = MPP_OK;
  if (this->type == MPP_VIDEO_CodingAVC) {
    MppPacket packet = NULL;
    ret = mApi->control(mCtx, MPP_ENC_GET_EXTRA_INFO, &packet);
    if (ret) {
      mpp_err("mpi control enc get extra info failed\n");
    }

    // get and write sps/pps for H.264
    if (packet) {
      void *ptr = mpp_packet_get_pos(packet);
      size_t len = mpp_packet_get_length(packet);

      SPS_buf = new unsigned char[len];
      // fwrite(ptr, 1, len, fp_out);
      memcpy(SPS_buf, ptr, len);
      SPS_length = len;
      packet = NULL;
    }
  }
  return ret;
}
MPP_RET Encoder::encode_YUV(cv::Mat yuvImg, unsigned char *&H264_buf,
                            size_t &length) {
  MPP_RET ret;

  MppFrame frame = NULL;
  MppPacket packet = NULL;
  void *buf = mpp_buffer_get_ptr(this->mBuffer);

  ret = read_yuv_buffer((RK_U8 *)buf, yuvImg, this->width, this->height);

  ret = mpp_frame_init(&frame);
  if (ret) {
    mpp_err_f("mpp_frame_init failed\n");
    goto RET;
  }

  mpp_frame_set_width(frame, this->width);
  mpp_frame_set_height(frame, this->height);
  mpp_frame_set_hor_stride(frame, this->hor_stride);
  mpp_frame_set_ver_stride(frame, this->ver_stride);
  mpp_frame_set_fmt(frame, this->fmt);
  mpp_frame_set_buffer(frame, this->mBuffer);
  mpp_frame_set_eos(frame, this->frm_eos);

  ret = this->mApi->encode_put_frame(mCtx, frame);

  if (ret) {
    mpp_err("mpp encode put frame failed\n");
    goto RET;
  }

  ret = this->mApi->encode_get_packet(mCtx, &packet);
  if (ret) {
    mpp_err("mpp encode get packet failed\n");
    goto RET;
  }

  if (packet) {
    // write packet to file here
    void *ptr = mpp_packet_get_pos(packet);
    size_t len = mpp_packet_get_length(packet);

    this->pkt_eos = mpp_packet_get_eos(packet);

    H264_buf = new unsigned char[len];

    memcpy(H264_buf, ptr, len);
    length = len;

    mpp_packet_deinit(&packet);
    this->stream_size += len;
    this->frame_count++;

    if (this->pkt_eos) {
      mpp_log("found last packet\n");
      mpp_assert(this->frm_eos);
    }
  }
RET:
  return ret;
}
int Encoder::YuvtoH264(int width, int height, cv::Mat yuv_frame,
                       unsigned char *(&encode_buf), int &encode_length) {

  if (this->frame_count == 0) {
    encode_Head(width, height, this->SPS_buf, this->SPS_length);
    encode_YUV(yuv_frame, this->H264_buf, this->H264_length);

    encode_buf = new unsigned char[this->SPS_length + this->H264_length];
    memcpy(encode_buf, this->SPS_buf, this->SPS_length);
    memcpy(encode_buf + this->SPS_length, this->H264_buf, this->H264_length);
    encode_length = this->H264_length + this->SPS_length;

    this->frame_count++;
    delete this->H264_buf;
    delete this->SPS_buf;
  } else {
    encode_YUV(yuv_frame, this->H264_buf, this->H264_length);
    encode_buf = new unsigned char[this->H264_length];
    memcpy(encode_buf, this->H264_buf, this->H264_length);
    encode_length = this->H264_length;
    this->frame_count++;
    delete this->H264_buf;
  }

  fwrite(encode_buf, 1, encode_length, this->mFout);
  return 0;
}

} // namespace MppEncoder
