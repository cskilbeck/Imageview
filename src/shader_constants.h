{
    // uv_scale/uv_offset for sampling the texture based on where it's dragged/zoomed to
    float2 uv_scale;
    float2 uv_offset;

    // color outside the image
    float4 border_color;

    // top left/bottom right of the image rectangle in screen coordinate
    int2 top_left;
    int2 bottom_right;

    // grid colors in a 2x2 checkerboard pattern
    float4 grid_color[4];

    // selection rectangle topleft/bottomright in screen coordinates
    int4 inner_select_rect;

    // selection rectangle expanded to contain the outline which may be >1 wide
    int4 outer_select_rect;

    float4 grid_overlay_color;

    // selection overlay color
    float4 select_color;

    // selection outline grid colors in a 2x2 checkerboard
    float4 select_outline_color[4];

    // colors for the crosshairs in a 2x2 checkerboard
    float4 line_color[4];

    // grid offset is {0,0} for 'floating', otherwise based on image position
    float2 grid_offset;

    // grid size in screen pixels
    float grid_size;

    // current frame for animating select outline/crosshairs
    int frame;

    // dash line length for select outline/crosshairs
    uint dash_length;

    // selection outline width in pixels
    int select_border_width;
}
