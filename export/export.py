import os
import urllib
import traceback
import time
import sys
import numpy as np
from rknn.api import RKNN
import torch.nn as nn
import torch
ONNX_MODEL = "./model_local.onnx"
RKNN_MODEL = "./model_local.rknn"
DATASET = "./dataset.txt"
IMAGE = "./dataset/test/test.jpg"
QUANTIZE_ON = False


def export_rknn_inference():
    # Create RKNN object
    rknn = RKNN(verbose=True)

    # pre-process config
    print("--> Config model")
    rknn.config(
        # mean_values=[[0, 0, 0]],
        # std_values=[[255, 255, 255]],
        target_platform="rk3588",
    )
    print("done")

    # Load ONNX model
    print("--> Loading model")
    ret = rknn.load_onnx(model=ONNX_MODEL)
    if ret != 0:
        print("Load model failed!")
        exit(ret)
    print("done")

    # Build model
    print("--> Building model")
    ret = rknn.build(do_quantization=QUANTIZE_ON, dataset=DATASET, rknn_batch_size=1)
    if ret != 0:
        print("Build model failed!")
        exit(ret)
    print("done")

    # Export RKNN model
    print("--> Export rknn model")
    ret = rknn.export_rknn(RKNN_MODEL)
    if ret != 0:
        print("Export rknn model failed!")
        exit(ret)
    print("done")
    print("`````````````````````````````````````````````````````````````")
    # Init runtime environment
    print("--> Init runtime environment")
    ret = rknn.init_runtime()
    # ret = rknn.init_runtime(target='rk3566')
    if ret != 0:
        print("Init runtime environment failed!")
        exit(ret)
    # inference here
    # inference here
    print("done")

    rknn.release()
    print("done")


if __name__ == "__main__":
    print("This is main ...")
    outputs = export_rknn_inference()
