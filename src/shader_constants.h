{
    // uv_scale/uv_offset for sampling the texture based on where it's dragged/zoomed to
    float2 uv_scale;
    float2 uv_offset;

    // actually the border color
    float4 background_color;

    int4 rect_f;

    // top left/bottom right of the image rectangle in screen coordinate
    float2 top_left;
    float2 bottom_right;

    // grid colors in a 2x2 checkerboard pattern
    float4 grid_color[4];

    // selection rectangle topleft/bottomright in screen coordinates
    int4 inner_select_rect;

    // selection rectangle expanded to contain the outline which may be >1 wide
    int4 outer_select_rect;

    // selection overlay color
    float4 select_color;

    // selection outline grid colors
    float4 select_outline_color[4];

    // colors for the crosshairs
    float4 line_color[4];

    // grid offset is {0,0} for 'floating', otherwise based on image position
    float2 grid_offset;

    float2 glowing_line_s;
    float2 glowing_line_e;

    float grid_size;

    // current frame for animating select outline/crosshairs
    int frame;

    // dash line length for select outline/crosshairs
    int dash_length;

    // selection outline width in pixels
    int select_border_width;
}
