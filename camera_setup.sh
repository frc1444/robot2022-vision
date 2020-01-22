#!/bin/sh

v4l2-ctl --device=1 --set-ctrl=auto_exposure=1 --set-ctrl=gain_automatic=0 --set-ctrl=white_balance_automatic=0
v4l2-ctl --device=1 --set-ctrl=exposure=30 --set-ctrl=gain=0 --set-ctrl=contrast=50


