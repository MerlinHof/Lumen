#include "text_input.h"
TextInputSpec text_input_spec(int target) {
    TextInputSpec s = {512, 0, 0, 0};
    switch (target) {
    case 0:
    case 61:
        s.capacity = 128;
        break;
    case 1:
        s.capacity = 241;
        break;
    case 2:
        s.capacity = 384;
        break;
    case 3:
        s.capacity = 192;
        break;
    case 4:
    case 160:
        s.capacity = 2048;
        break;
    case 6:
    case 150:
    case 172:
        s.capacity = 256;
        break;
    case 8:
        s = (TextInputSpec){512, 1, 0, 0};
        break;
    case 112:
        s = (TextInputSpec){512, 1, 0, 0};
        break;
    case 162:
        s.capacity = 96;
        break;
    case 30:
    case 60:
    case 171:
    case 174:
    case 177:
        s = (TextInputSpec){8192, 1, 0, 0};
        break;
    case 110:
    case 180:
        s = (TextInputSpec){200, 1, 0, 0};
        break;
    case 170:
    case 111:
        s.capacity = 64;
        break;
    case 179:
        s.capacity = 16;
        break;
    case 178:
    case 176:
        s.capacity = 2048;
        break;
    case 175:
        s.capacity = 100;
        break;
    case 100:
        s.capacity = 6;
        break;
    case 113:
        s = (TextInputSpec){1024, 1, 0, 0};
        break;
    case 120:
        s.capacity = 96;
        break;
    case 33:
    case 34:
        s.capacity = 256;
        break;
    case 35:
        s.capacity = 33;
        break;
    case 36:
    case 37:
        s.capacity = 9;
        break;
    case 38:
        s.capacity = 4;
        break;
    case 40:
        s.capacity = 8;
        break;
    }
    s.gutter = target == 30 ? 20 : 0;
    s.secret = target == 5 || target == 32;
    return s;
}
