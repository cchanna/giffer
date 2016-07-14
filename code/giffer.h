struct GIFFER_IMAGE
{
    uint8_t* values;
    uint8_t* previous_values;
    uint16_t frame_width, frame_height;
    uint16_t left, right, top, bottom, width, height;
    int current_pixel;
};

struct GIFFER_COLOR
{
    uint8_t r,g,b;
};

struct GIFFER_COLOR_CUBE
{
    int subcube[2][2][2];
    uint64_t amount;
    uint8_t subcubes;
    uint8_t color_index;
    GIFFER_COLOR color;
};

struct GIFFER_COLOR_CUBE_HEAD
{
    GIFFER_COLOR_CUBE* cubes;
    int size;
    uint64_t max_size;
};

struct GIFFER_LZW_NODE
{
	uint16_t next[256];
};

struct GIFFER_MEMORY
{
    GIFFER_IMAGE image;
    uint64_t frames;
    uint64_t max_frames;
    uint8_t* video;
    uint16_t width, height;
    uint16_t delay;
    uint16_t color_table_size;
    uint8_t color_table_size_compressed;
    int* color_heap;
    GIFFER_COLOR* color_table;
    GIFFER_COLOR_CUBE_HEAD color_cube;
    uint64_t code_tree_memory_size;
    GIFFER_LZW_NODE* code_tree;
};


// #define GIFFER_INIT(name) GIFFER_MEMORY* name(uint16_t width, uint16_t height, uint8_t color_table_size, uint16_t delay)
// typedef GIFFER_INIT(giffer_init);
GIFFER_MEMORY* Giffer_Init(uint16_t width, uint16_t height, uint8_t color_table_size, uint16_t delay);

void Giffer_Frame(GIFFER_MEMORY* memory, uint8_t* frame);

void Giffer_End(GIFFER_MEMORY* memory);
