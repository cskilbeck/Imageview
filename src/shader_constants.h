
// for draw_rectangle
float2 rect_position;
float2 rect_size;

// colors[0] for solid shader
// colors[0], [1] for stripey shader
// colors[0],[1],[2],[3] for checkerboard
float4 colors[4];

// checkerboard offset is {0,0} for 'floating', otherwise based on image position
float2 checkerboard_offset;

// 1.0 / grid size in screen pixels
// 1.0 / stripe length in screen pixels
float check_strip_size;

// current frame for animating stripe shader
float frame;

// 1.0 / stripe_length for stripe shader
float stripe_length;
