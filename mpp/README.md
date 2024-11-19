## RK3588 Mpp decoder/encoder

### Compile

```bash
git clone git@github.com:Sunhaihua1/MppCoding.git
git checkout dev
cmake -B build
cmake --build build
```

### Run

1. copy `build/ouput` directory to RK3588 board (if cross-compile on PC)

2. run in 3588

   ```bash
   cd build/output
   export LD_LIBRARY_PATH=./lib
   ./mpp_test
   ```

### test

```bash
#rtsp stream:
cvlc trailer.mp4 --sout '#rtp{sdp=rtsp://10.42.0.1:8554/live}' --loop
```
