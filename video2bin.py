#!/usr/bin/env python3
import sys
import cv2 as cv
from oled.img2code import convert_frame_to_bytes

DEFAULT_PORT = 5678

def convert_video_to_bin(videoFile, binFile):
    cap = cv.VideoCapture(videoFile)
    frameCount = cap.get(cv.CAP_PROP_FRAME_COUNT)
    print('frame count:', frameCount)
    print('frame width:', cap.get(cv.CAP_PROP_FRAME_WIDTH))
    print('frame height:', cap.get(cv.CAP_PROP_FRAME_HEIGHT))
    lastPercent = 0
    with open(binFile, 'wb+') as f:
        while True:
            retval, frame = cap.read()
            if not retval:
                print('video done!')
                break
            bitmap = convert_frame_to_bytes(frame)
            f.write(bitmap)
            pos = cap.get(cv.CAP_PROP_POS_FRAMES)
            percent = pos /  frameCount * 100
            if percent - lastPercent >= 1:
                lastPercent = percent
                sys.stdout.write('=')
                sys.stdout.flush()
    print('convert all frames done!')
    cap.release()

def main():
    if len(sys.argv) < 3:
        print("Usage: {} videoFile binFile\n\t".format(sys.argv[0]))
        exit(-1)

    try:
        videoFile = sys.argv[1]
        binFile = sys.argv[2]
        convert_video_to_bin(videoFile, binFile)
    except Exception as e:
        print('exception raised:', e)

if __name__ == "__main__":
    main()
