#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>




struct GIF_WRITER
{
    FILE* file;
    uint8_t data_block[255];
    uint8_t block_length;
    uint8_t byte;
    uint8_t bits_written;
};




#define MAX_CODE_TREE_SIZE 4096

#include "giffer.h"

inline void
Write8(GIF_WRITER *writer, uint8_t byte)
{
	fputc(byte, writer->file);
}

inline void
Write16(GIF_WRITER *writer, uint16_t value)
{
	fputc((uint8_t) value, writer->file);
	fputc((uint8_t) (value >> 8), writer->file);
}

inline void
WriteString(GIF_WRITER *writer, char *string)
{
	while(*string)
	{
		fputc(*(string++),writer->file);
	}
}

void
WriteBlock(GIF_WRITER *writer)
{
	Write8(writer,writer->block_length);
	for (int i = 0; i < writer->block_length; i++)
	{
		Write8(writer, writer->data_block[i]);
	}
	writer->block_length = 0;
}

inline void
WriteByteToBlock(GIF_WRITER *writer)
{
	writer->data_block[writer->block_length++] = writer->byte;
	writer->byte = 0;
	if (writer->block_length == 255)
	{
		WriteBlock(writer);
	}
}

void
WriteCode(GIF_WRITER *writer, int32_t code, uint8_t current_code_size)
{
	writer->byte = (uint8_t)(writer->byte | (code << writer->bits_written));
	writer->bits_written += current_code_size;
	while (writer->bits_written >= 8)
	{
		WriteByteToBlock(writer);
		writer->byte = (uint8_t)(code >> (8 - writer->bits_written + current_code_size));
		writer->bits_written -= 8;
	}
}


int GetSubcube(GIFFER_COLOR_CUBE_HEAD *c, int cube, int level, GIFFER_COLOR color)
{
    GIFFER_COLOR_CUBE* cubes;
    int r,g,b;
    int subcube;

    cubes = c->cubes;
    r = (color.r >> level) & 1;
    g = (color.g >> level) & 1;
    b = (color.b >> level) & 1;
    subcube = cubes[cube].subcube[r][g][b];
    if (subcube == 0)
    {
        cubes[cube].subcubes++;
        if (c->size == c->max_size)
        {
            uint64_t old_color_cube_size = sizeof(GIFFER_COLOR_CUBE) * c->max_size;
            c->max_size *= 2;
            uint64_t new_color_cube_size = sizeof(GIFFER_COLOR_CUBE) * c->max_size;
            GIFFER_COLOR_CUBE *cubes_tmp = (GIFFER_COLOR_CUBE*) malloc(new_color_cube_size);
            memset(cubes_tmp, 0, new_color_cube_size);
            memcpy(cubes_tmp, cubes, old_color_cube_size);
            free(cubes);
            c->cubes = cubes_tmp;
            cubes = c->cubes;
        }
        subcube = c->size;
        cubes[cube].subcube[r][g][b] = subcube;
    }
    return subcube;
}

void CubeInsert(GIFFER_COLOR_CUBE_HEAD *c, int cube, int level, GIFFER_COLOR color, uint64_t amount)
{
    GIFFER_COLOR_CUBE* cubes;

    if (cube == -1)
    {
        cube = 0;
    }
    else
    {
        cube = GetSubcube(c,cube,level,color);
    }
    cubes = c->cubes;
    if (cubes[cube].amount == 0)
    {
        cubes[cube].color = color;
		c->size++;
    }
    cubes[cube].amount += amount;
    if (cubes[cube].subcubes)
    {
        CubeInsert(c, cube, level-1, color, 1);
    }
    else
    {
        if (
            cubes[cube].color.r == color.r &&
            cubes[cube].color.g == color.g &&
            cubes[cube].color.b == color.b
        ){
            // do nothing
        }
        else
        {
            CubeInsert(c, cube, level-1, cubes[cube].color, cubes[cube].amount - amount);
            CubeInsert(c, cube, level-1, color, 1);
        }
    }
}

inline void CubeInsert(GIFFER_COLOR_CUBE_HEAD *c, GIFFER_COLOR color)
{
    CubeInsert(c, -1, 7, color, 1);
}

void SortCubes(GIFFER_COLOR_CUBE *cubes, int cube)
{
    int highest_cube = 0;
    uint64_t highest_amount = 0;
    uint64_t total_amount = 0;
    if (cubes[cube].subcubes)
    {
        for (int r=0; r<2; r++)
        {
            for (int g=0; g<2; g++)
            {
                for (int b=0; b<2; b++)
                {
                    int subcube = cubes[cube].subcube[r][g][b];
                    if (subcube > 0)
                    {
                        SortCubes(cubes, subcube);
                        total_amount += cubes[subcube].amount;
                        if (cubes[subcube].amount > highest_amount)
                        {
                            highest_cube = subcube;
                            highest_amount = cubes[subcube].amount;
                        }
                    }
                }
            }
        }
        cubes[cube].color = cubes[highest_cube].color;
    }
}

inline void SortCubes(GIFFER_COLOR_CUBE *cubes)
{
    SortCubes(cubes, 0);
}

uint8_t
CubeSearch(GIFFER_COLOR_CUBE_HEAD *c, int cube, int level, GIFFER_COLOR color)
{
	GIFFER_COLOR_CUBE *cubes = c->cubes;
	if (cube == -1)
	{
		cube = 0;
	}
	else
	{
		int subcube = GetSubcube(c,cube,level,color);
		cube = subcube;
	}
	if (cubes[cube].subcubes)
	{
		return CubeSearch(c,cube,level - 1, color);
	}
	else
	{
		return cubes[cube].color_index;
	}
}

inline uint8_t
CubeSearch(GIFFER_COLOR_CUBE_HEAD *c, GIFFER_COLOR color)
{
	return CubeSearch(c,-1,7,color);
}

void HeapSwap(int* heap, int node_1, int node_2)
{
	int temp = heap[node_1];
	heap[node_1] = heap[node_2];
	heap[node_2] = temp;
}

void HeapPush(int* heap, int cube, GIFFER_COLOR_CUBE* cubes)
{
    int current_node, parent_node;

    heap[0]++;
    current_node = heap[0];
    heap[current_node] = cube;
    parent_node = current_node/2;
    while (current_node > 1 && cubes[heap[parent_node]].amount < cubes[heap[current_node]].amount)
    {
        HeapSwap(heap, current_node, parent_node);
        current_node = parent_node;
        parent_node = current_node/2;
    }
}

int HeapPop(int* heap, GIFFER_COLOR_CUBE* cubes)
{
    int current_node, return_value;

    current_node = 1;
    return_value = heap[current_node];
    heap[current_node] = heap[heap[0]];
    heap[heap[0]] = 0;
    heap[0]--;
    while (current_node <= heap[0])
    {
		int child_1 = current_node * 2;
		if (child_1 <= heap[0])
		{
			int greater_child = child_1;
			int child_2 = (current_node * 2) + 1;
			if (child_2 <= heap[0] && cubes[heap[child_2]].amount > cubes[heap[child_1]].amount)
			{
				greater_child = child_2;
			}
			HeapSwap(heap,current_node,greater_child);
			current_node = greater_child;
		}
		else
		{
			break;
		}
	}
	return return_value;
}

uint8_t GetNextValue(GIFFER_IMAGE *image)
{
	int i = 0;
	i += ((image->current_pixel % image->width) + image->left); // NOTE(cch):x
	i += (((image->current_pixel / image->width) + image->top) * image->frame_width); // NOTE(cch):y
	image->current_pixel++;
	image->current_pixel %= image->width * image->height;
	return image->values[i];
}

GIFFER_MEMORY* Giffer_Init(uint16_t width, uint16_t height, uint8_t color_table_size, uint16_t delay)
{
    GIFFER_MEMORY* memory = {};
    uint64_t color_heap_size, color_table_memory_size, color_cube_size, video_size;
    memory = (GIFFER_MEMORY*) malloc(sizeof(GIFFER_MEMORY));

    memory->width = width;
    memory->height = height;
    memory->delay = delay;
    memory->color_table_size_compressed = color_table_size;
    memory->color_table_size = 2 << memory->color_table_size_compressed;
    memory->frames = 0;
    memory->max_frames = 60;
    memory->color_cube = {};
    memory->color_cube.max_size = 2048;


    color_heap_size = sizeof(int) * (memory->color_table_size + 1);
    memory->color_heap = (int*) malloc(color_heap_size);
    memset(memory->color_heap, 0, color_heap_size);

    color_table_memory_size = sizeof(GIFFER_COLOR) * (memory->color_table_size);
    memory->color_table = (GIFFER_COLOR*) malloc(color_table_memory_size);
    memset(memory->color_table, 0, color_table_memory_size);

    color_cube_size = sizeof(GIFFER_COLOR_CUBE) * memory->color_cube.max_size;
    memory->color_cube.cubes = (GIFFER_COLOR_CUBE*) malloc(color_cube_size);
    memset(memory->color_cube.cubes, 0, color_cube_size);

    video_size = sizeof(uint8_t) * memory->width * memory->height * memory->max_frames * 4;
    memory->video = (uint8_t*) malloc(video_size);
    memset(memory->video, 0, video_size);

    memory->code_tree_memory_size = sizeof(GIFFER_LZW_NODE) * MAX_CODE_TREE_SIZE;
    memory->code_tree = (GIFFER_LZW_NODE*) malloc(memory->code_tree_memory_size);
    memset(memory->code_tree, 0, memory->code_tree_memory_size);

    return memory;
}

void Giffer_Frame(GIFFER_MEMORY* memory, uint8_t* frame)
{
    uint8_t* pixel;
    uint8_t* new_frame_position;
    int current_pixel;

    memory->frames++;
    if (memory->frames >= memory->max_frames)
    {
        uint64_t old_video_size, new_video_size;
        uint8_t* video_tmp;

        old_video_size = sizeof(uint8_t) * 4 * memory->width * memory->height * memory->max_frames;
        memory->max_frames *= 2;
        new_video_size = sizeof(uint8_t) * 4 * memory->width * memory->height * memory->max_frames;
        video_tmp = (uint8_t*) malloc(new_video_size);
        memset(video_tmp, 0, new_video_size);
        memcpy(video_tmp, memory->video, old_video_size);
        free(memory->video);
        memory->video = video_tmp;
    }
    pixel = frame;
    current_pixel = 0;
    for (int i=0; i < memory->width * memory->height; i++)
    {
        GIFFER_COLOR color;

        color.r = *pixel++;
        color.g = *pixel++;
        color.b = *pixel++;
        *pixel++;
        CubeInsert(&(memory->color_cube), color);
    }
    new_frame_position = memory->video + (4 * memory->width * memory->height * (memory->frames - 1));
    memcpy(new_frame_position, frame, memory->width * memory->height * 4);
}

void Giffer_End(GIFFER_MEMORY* memory)
{
    int num_colors = 0;
    SortCubes(memory->color_cube.cubes);
    {
        GIFFER_COLOR_CUBE* cubes = memory->color_cube.cubes;
        HeapPush(memory->color_heap, 0, memory->color_cube.cubes);
        while (memory->color_heap[0] > 0)
        {
            int cube = HeapPop(memory->color_heap, cubes);
            if (
                cubes[cube].subcubes > 0 &&
                memory->color_heap[0] + num_colors + cubes[cube].subcubes <= memory->color_table_size
            )
            {
                for (int r = 0; r < 2; r++)
                {
                    for (int g = 0; g < 2; g++)
                    {
                        for (int b = 0; b < 2; b++)
                        {
                            int subcube = cubes[cube].subcube[r][g][b];
                            if (subcube > 0)
                            {
                                HeapPush(memory->color_heap, subcube, cubes);
                            }
                        }
                    }
                }
            }
            else
            {
                cubes[cube].subcubes = 0;
                cubes[cube].color_index = (uint8_t) num_colors;
                memory->color_table[num_colors] = cubes[cube].color;
                num_colors++;
            }
        }
    }


    GIFFER_IMAGE image;
    image.frame_width = memory->width;
    image.frame_height = memory->height;
    image.current_pixel = 0;

    uint64_t pixels_in_frame = memory->width * memory->height;
    image.values = (uint8_t*) malloc(pixels_in_frame);
    memset(image.values, 0, pixels_in_frame);
    image.previous_values = (uint8_t*) malloc(pixels_in_frame);
    memset(image.previous_values, 0, pixels_in_frame);

    uint8_t* pixel;

    GIF_WRITER w = {};
    w.file = 0;
    fopen_s(&(w.file), "giffer.gif", "wb");
    if (w.file == 0)
    {
        return;
    }

    // NOTE(cch): header
	WriteString(&w,"GIF89a");

	// NOTE(cch): logical screen descriptor
	Write16(&w,memory->width);
	Write16(&w,memory->height);
	Write8(&w,(1 << 7) | (1 << 4) | memory->color_table_size_compressed);
	Write8(&w,0x00); // NOTE(cch): background color index
	Write8(&w,0x00); // NOTE(cch): pixel aspect ratio

	// NOTE(cch): global color table
	for (int i = 0; i < memory->color_table_size; i++)
	{
		Write8(&w,memory->color_table[i].r);
		Write8(&w,memory->color_table[i].g);
		Write8(&w,memory->color_table[i].b);
	}

	// NOTE(cch) application extension (animation)
	Write8(&w,0x21); // NOTE(cch): extension introducer
	Write8(&w,0xFF); // NOTE(cch): application extension label
	Write8(&w,11); // NOTE(cch): eleven bytes follow
	WriteString(&w,"NETSCAPE2.0");
	Write8(&w,3); // NOTE(cch): three bytes follow
	Write8(&w,0x01); // NOTE(cch): *shrug*
	Write16(&w,0); // NOTE(cch): repeat forever
	Write8(&w,0x00); // NOTE(cch): block terminator

	pixel = memory->video;
	for (int current_frame = 0; current_frame < memory->frames; current_frame++)
	{
        image.left = memory->width - 1;
		image.right = 0;
		image.top = memory->height - 1;
		image.bottom = 0;
        int is_different = 0;
		{
			for (int i = 0; i < memory->width * memory->height; i++)
			{
				GIFFER_COLOR color;
				color.r = *pixel++;
				color.g = *pixel++;
				color.b = *pixel++;
				*pixel++;
				uint8_t index = CubeSearch(&(memory->color_cube),color);
				image.values[i] = index;
				if (current_frame > 0)
				{
					if (image.values[i] != image.previous_values[i])
					{
                        is_different = true;
						uint16_t x = i % memory->width;
						if (x < image.left)
						{
							image.left = x;
						}
						if (x > image.right)
						{
							image.right = x;
						}
						uint16_t y = (uint16_t) (i / memory->width);
						if (y < image.top)
						{
							image.top = y;
						}
						if (y > image.bottom)
						{
							image.bottom = y;
						}
					}
				}
			}
			if (is_different)
			{
				image.width = image.right - image.left + 1;
				image.height = image.bottom - image.top + 1;
			}
			else
			{
				image.left = 0;
				image.top = 0;
				image.width = memory->width;
				image.height = memory->height;
			}
		}
		// NOTE(cch): graphics control extension
		Write8(&w,0x21); // NOTE(cch): extension introducer
		Write8(&w,0xF9); // NOTE(cch): graphic control label
		Write8(&w,0x04); // NOTE(cch): byte size
		uint8_t packaged_field = 0;
		if (0)
		{
			packaged_field = packaged_field | 1;
		}
		packaged_field = packaged_field | (1 << 2); // NOTE(cch): disposal method
		Write8(&w,packaged_field);
		Write16(&w,memory->delay);
		Write8(&w,0x00); // NOTE(cch): transparent color index
		Write8(&w,0x00); // NOTE(cch): block terminator

		// NOTE(cch): image descriptor
		Write8(&w,0x2C);
		Write16(&w,image.left);   // NOTE(cch): image left
		Write16(&w,image.top);   // NOTE(cch): image top
		Write16(&w,image.width);  // NOTE(cch): image width
		Write16(&w,image.height);  // NOTE(cch): image height
		Write8(&w,0x00); // NOTE(cch): no local color table

		// NOTE(cch): image data
        uint8_t lzw_minimum_code_size = memory->color_table_size_compressed + 1;
		Write8(&w,lzw_minimum_code_size);

		// NOTE(cch): creating the code stream
		uint16_t tree_size = memory->color_table_size;
		uint16_t clear_code = tree_size++;
		uint16_t end_of_information_code = tree_size++;
		uint8_t current_code_size = lzw_minimum_code_size + 1;
		WriteCode(&w, clear_code, current_code_size);

		int32_t current_code = GetNextValue(&(image));
		for (int i = 1; i < image.width * image.height; i++)
		{
			uint16_t next_index = GetNextValue(&(image));

			// NOTE(cch): search for matching code run in code tree
			if (memory->code_tree[current_code].next[next_index])
			{
				current_code = memory->code_tree[current_code].next[next_index];
			}
			else
			{
				WriteCode(&w, current_code, current_code_size);
				if (tree_size == (1 << current_code_size))
				{
					current_code_size++;
				}
				memory->code_tree[current_code].next[next_index] = tree_size++;

				// NOTE(cch): if the code table reaches the maximum size, we
				// start fresh with a new table
				if (tree_size == MAX_CODE_TREE_SIZE)
				{
					WriteCode(&w, clear_code, current_code_size);
					memset(memory->code_tree,0,memory->code_tree_memory_size);
					current_code_size = lzw_minimum_code_size + 1;
					tree_size = end_of_information_code + 1;
				}
				current_code = next_index;
			}
		}
		memset(memory->code_tree,0,memory->code_tree_memory_size);
		WriteCode(&w, current_code, current_code_size);
		WriteCode(&w, clear_code, current_code_size);
		WriteCode(&w, end_of_information_code, current_code_size);
		if (w.bits_written > 0)
		{
			WriteByteToBlock(&w);
			w.bits_written = 0;
		}
		if (w.block_length > 0)
		{
			WriteBlock(&w);
		}
		Write8(&w,0x00); //NOTE(cch): block terminator
		uint8_t *values_tmp = image.previous_values;
		image.previous_values = image.values;
		image.values = values_tmp;
    }
    // NOTE(cch): trailer
    Write8(&w,0x3B);
    fclose(w.file);

    free(image.values);
    free(image.previous_values);
    free(memory->color_cube.cubes);
    free(memory->video);
    free(memory->color_heap);
    free(memory->color_table);
    free(memory->code_tree);
    free(memory);
}
