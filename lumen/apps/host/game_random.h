static uint32_t pro_random(void) {
    pro_rng = pro_rng * 1664525u + 1013904223u;
    return pro_rng;
}
