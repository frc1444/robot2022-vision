#!/bin/sh

v4l2-ctl --device=2 --set-ctrl=auto_exposure=1 --set-ctrl=gain_automatic=0 --set-ctrl=white_balance_automatic=0 --set-ctrl=horizontal_flip=1 --set-ctrl=vertical_flip=1
v4l2-ctl --device=2 --set-ctrl=exposure=5 --set-ctrl=gain=0 --set-ctrl=contrast=50


