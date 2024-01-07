// this file is shared between the CPU and GPU so alignment matters
// maintain 16 byte (4 x 32) alignment for each field
// you can pack things into 16 byte structs if that makes things easier

// for image flip/rotate
matrix texture_transform;

// for draw_rectangle
float4 rect;

// colors[0] for solid shader
// colors[0], [1] for stripey shader
// colors[0],[1],[2],[3] for checkerboard
float4 colors[4];

// apparently up to 4 trailing floats can be specified without packing into a struct

// checkerboard offset is {0,0} for 'floating', otherwise based on image position
float2 checkerboard_offset;

// 1.0 / grid size in screen pixels
// 1.0 / stripe length in screen pixels
float check_strip_size;

// current frame for animating stripe shader
float frame;
