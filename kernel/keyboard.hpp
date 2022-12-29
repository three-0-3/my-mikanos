#pragma once

#include <deque>
#include "message.hpp"

const int kLControlBitMask = 0b00000001u;
const int kLShiftBitMask   = 0b00000010u;
const int kLAltBitMask     = 0b00000100u;
const int kLGUIBitMask     = 0b00001000u;
const int kRControlBitMask = 0b00010000u;
const int kRShiftBitMask   = 0b00100000u;
const int kRAltBitMask     = 0b01000000u;
const int kRGUIBitMask     = 0b10000000u;

void InitializeKeyboard();