{
    // uv_scale/uv_offset for sampling the texture based on where it's dragged/zoomed to
    float2 uv_scale;
    float2 uv_offset;

    // color outside the image
    float4 border_color;

    // top left/bottom right of the image rectangle in screen coordinate
    float4 image_rect;

    // grid colors in a 2x2 checkerboard pattern
    float4 checkerboard_color[4];

    // selection rectangle topleft/bottomright in screen coordinates
    float4 inner_select_rect;

    // selection rectangle expanded to contain the outline which may be >1 wide
    float4 outer_select_rect;

    // selection overlay color
    float4 select_color;

    // selection outline grid colors in a 2x2 checkerboard
    float4 select_outline_color[2];

    float4 crosshair_color[2];

    // x,y in pixels of crosshairs (set to -1,-1 to deactivate)
    float2 crosshairs;

    // grid offset is {0,0} for 'floating', otherwise based on image position
    float2 grid_offset;

    float2 crosshair_width;

    // 1.0 / grid size in screen pixels
    float grid_size;

    // current frame for animating select outline
    float select_frame;

    // 1.0 / dash line length for select outline
    float select_dash_length;

    // current frame for animating crosshair
    float crosshair_frame;

    // 1.0 / dash line length for crosshair
    float crosshair_dash_length;
}
