#pragma once

// Layout math only (96-dpi design units scaled by dpi). Used by the GUI and logic tests.
struct ModuleBlock {
    int group_x = 0, group_y = 0, group_w = 0, group_h = 0;
    int all_y = 0;
    int radio_y[8]{};
    int radio_h = 20;
    int radio_x = 0, radio_w = 0;
    int nmod = 0;
    int below_y = 0;
};

inline int scale_px(int v, int dpi) {
    if (dpi <= 0) dpi = 96;
    return (v * dpi + 48) / 96;
}

// nmod = detected modules (0..8). Always includes the "All modules" row.
inline ModuleBlock module_block(int nmod, int dpi, int group_y) {
    if (nmod < 0) nmod = 0;
    if (nmod > 8) nmod = 8;
    ModuleBlock b;
    b.nmod = nmod;
    b.group_x = scale_px(12, dpi);
    b.group_y = group_y;
    b.group_w = scale_px(520, dpi);
    b.radio_x = scale_px(24, dpi);
    b.radio_w = scale_px(480, dpi);
    b.radio_h = scale_px(20, dpi);
    int title = scale_px(20, dpi);
    int gap = scale_px(22, dpi);
    int pad = scale_px(10, dpi);
    b.all_y = group_y + title;
    for (int i = 0; i < nmod; i++)
        b.radio_y[i] = b.all_y + gap * (i + 1);
    int last_bottom = b.all_y + b.radio_h;
    if (nmod > 0)
        last_bottom = b.radio_y[nmod - 1] + b.radio_h;
    b.group_h = (last_bottom + pad) - group_y;
    if (b.group_h < title + b.radio_h + pad)
        b.group_h = title + b.radio_h + pad;
    b.below_y = b.group_y + b.group_h + scale_px(12, dpi);
    return b;
}

inline bool module_block_contains_radios(const ModuleBlock& b) {
    auto inside = [&](int y, int h) {
        return y >= b.group_y && (y + h) <= (b.group_y + b.group_h);
    };
    if (!inside(b.all_y, b.radio_h)) return false;
    for (int i = 0; i < b.nmod; i++)
        if (!inside(b.radio_y[i], b.radio_h)) return false;
    return b.below_y >= b.group_y + b.group_h;
}
