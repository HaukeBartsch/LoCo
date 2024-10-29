/*
 ./renderStory -d data -o /tmp/ -c forwardModel.json \
                -t 1
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <codecvt>
#include <exception>
#include <filesystem>
#include <fstream>
// #include <stxutif.h>

#include <ft2build.h>
#include FT_FREETYPE_H

// boost libraries
//#include <boost/property_tree/json_parser.hpp>
//#include <boost/property_tree/ptree.hpp>

#include "boost/date_time.hpp"
#include "boost/filesystem.hpp"
// we can also support the xml output
//#include <boost/property_tree/xml_parser.hpp>

// argument parsing
#include "json.hpp"
#include "optionparser.h"

// export as an image
#include <boost/gil.hpp>
#include <boost/gil/color_convert.hpp>
#include <boost/gil/extension/dynamic_image/any_image.hpp>
#include <boost/gil/extension/io/png.hpp>
#include <boost/gil/image_view.hpp>
#include <boost/gil/image_view_factory.hpp>
#include <boost/gil/planar_pixel_reference.hpp>
#include <boost/gil/typedefs.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/program_options.hpp>

// #include <boost/gil/extension/io/png_io.hpp>
// #include <boost/gil/extension/io/png_dynamic_io.hpp>
using namespace boost::gil;
using namespace boost::filesystem;

namespace po = boost::program_options;
bool verbose = false;

// Short alias for this namespace
using json = nlohmann::json;

// characters are going to be written into this one, so this depends on the font size we select, 28 is quite small... 
#define WIDTH 680
#define HEIGHT 28

/* origin is the upper left corner */
unsigned char image_buffer[HEIGHT][WIDTH];

/* Replace this function with something useful. */

void draw_bitmap(FT_Bitmap *bitmap, FT_Int x, FT_Int y) {
    FT_Int i, j, p, q;
    FT_Int x_max = x + bitmap->width;
    FT_Int y_max = y + bitmap->rows;

    for (i = x, p = 0; i < x_max; i++, p++) {
        for (j = y, q = 0; j < y_max; j++, q++) {
            if (i < 0 || j < 0 || i >= WIDTH || j >= HEIGHT)
                continue;

            image_buffer[j][i] |= bitmap->buffer[q * bitmap->width + p];
        }
    }
}

void show_image(void) {
    int i, j;

    for (i = 0; i < HEIGHT; i++) {
        for (j = 0; j < WIDTH; j++)
            putchar(image_buffer[i][j] == 0 ? ' ' : image_buffer[i][j] < 64 ? '.'
                                                                            : (image_buffer[i][j] < 128 ? '+' : '*'));
        putchar('\n');
    }
}

void show_json(char *c) {
    int i, j;

    int label[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
                   'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
                   'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
                   'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'};
    fprintf(stdout, "{ \"label\": [ ");
    int sizeOfLabel = sizeof(label) / sizeof(*label);
    for (int i = 0; i < sizeOfLabel; i++) {
        if (label[i] == (int)c[0])
            fprintf(stdout, "1");
        else
            fprintf(stdout, "0");
        if (i < sizeOfLabel - 1)
            fprintf(stdout, ",");
    }
    fprintf(stdout, " ],\n");

    fprintf(stdout, "  \"image\": [ ");
    for (i = 0; i < HEIGHT; i++) {
        for (j = 0; j < WIDTH; j++) {
            fprintf(stdout, "%d", (int)image_buffer[i][j]);
            if (!((i == HEIGHT - 1 && j == WIDTH - 1)))
                fprintf(stdout, ",");
        }
    }
    fprintf(stdout, " ] }\n");
}

struct Arg : public option::Arg {
    static option::ArgStatus Required(const option::Option &option, bool) {
        return option.arg == 0 ? option::ARG_ILLEGAL : option::ARG_OK;
    }
    static option::ArgStatus Empty(const option::Option &option, bool) {
        return (option.arg == 0 || option.arg[0] == 0) ? option::ARG_OK : option::ARG_IGNORE;
    }
};

// The idea is to load all the glyphs from a predefined text and check
// if any one of them is missing. Return the proportion of missing glyphs [0..1].
float proportionOfGlyphsMissing(std::string font_file_name, int face_index, int font_size) {
    std::vector<std::u32string> unicodeChars = {
        U"0", U"1", U"2", U"3", U"4", U"5", U"6", U"7", U"8", U"9", U" ", U" ", U" ", U" ",
        U" ", U" ", U"/", U"_", U"-", U".", U"A", U"B", U"C", U"D", U"E", U"F", U"G", U"H",
        U"I", U"J", U"K", U"L", U"M", U"N", U"O", U"P", U"Q", U"R", U"S", U"T", U"U", U"V",
        U"W", U"X", U"Y", U"Z", U"a", U"b", U"c", U"d", U"e", U"f", U"g", U"h", U"i", U"j",
        U"k", U"l", U"m", U"n", U"o", U"p", U"q", U"r", U"s", U"t", U"u", U"v", U"w", U"x",
        U"y", U"z", U"ø", U"æ", U"å", U"ö", U"ü", U"ä", U"(", U")", U"]", U"["};
    FT_Library library;
    FT_Face face;
    FT_GlyphSlot slot;
    FT_Matrix matrix; /* transformation matrix */
    // FT_Vector pen;    /* untransformed origin  */
    FT_Error error;
    float angle = 0;  // ( 25.0 / 360 ) * 3.14159 * 2;      /* use 25 degrees     */
    // int target_height = HEIGHT;

    error = FT_Init_FreeType(&library); /* initialize library */
    /* error handling omitted */
    if (error != 0) {
        fprintf(stderr,
                "Error: The freetype library could not be initialized with this font.\n");
        exit(-1);
    }

    error = FT_New_Face(library, font_file_name.c_str(), face_index, &face); /* create face object */
    if (face == NULL) {
        fprintf(stderr, "Error: no face found, provide the filename of a ttf file...\n");
        return 1.0;  // could not even get started
    }
    float font_size_in_pixel = font_size;
    error = FT_Set_Char_Size(face, font_size_in_pixel * 64, 0, 96, 0); /* set character size */
    if (error != 0) {
        return 1.0;  // could not even get started with this font and the given size
    }
    slot = face->glyph;

    /* set up matrix */
    matrix.xx = (FT_Fixed)(cos(angle) * 0x10000L);
    matrix.xy = (FT_Fixed)(-sin(angle) * 0x10000L);
    matrix.yx = (FT_Fixed)(sin(angle) * 0x10000L);
    matrix.yy = (FT_Fixed)(cos(angle) * 0x10000L);

    /* the pen position in 26.6 cartesian space coordinates; */
    /* start at (300,200) relative to the upper left corner  */
    // pen.x = 1 * 64;
    // pen.y = (target_height - 20) * 64;

    float countMissing = 0;
    for (int n = 0; n < unicodeChars.size(); n++) {
        /* set transformation */
        // FT_Set_Transform(face, &matrix, &pen);
        /* load glyph image into the slot (erase previous one) */
        // unsigned long c = FT_Get_Char_Index(face, text2print[n]);
        // error = FT_Load_Glyph(face, c, FT_LOAD_RENDER);
        if (FT_Get_Char_Index(face, unicodeChars[n][0]) == 0) {
            countMissing += 1.0f;
            continue;
        }

        error = FT_Load_Char(face, unicodeChars[n][0], FT_LOAD_RENDER);
        if (error)
            countMissing += 1.0f;
    }
    // everything is fine
    fprintf(stdout, "font check: %0.2f%% characters missing for %s [font size: %d]\n", 100.0f * (countMissing / unicodeChars.size()), font_file_name.c_str(), font_size);
    return countMissing / unicodeChars.size();
}


int main(int argc, char **argv) {
    setlocale(LC_NUMERIC, "en_US.utf-8");
    std::string font_path = "Roboto-Regular.ttf";
    std::string output_path("data");
    std::string story_file = "../stories.json";
    int font_size = 12;

    po::options_description desc("renderStory: Write out a story as a series of images.\n\nExample:\n  renderStory --verbose ../stories.json\n\nAllowed options");
        desc.add_options()
          ("help,h", "Print this help.")
          ("font,f", po::value< std::string >(&font_path), "The font file (ttf) to be used [Roboto-Regular.ttf].")
          ("font_size,s", po::value< int >(&font_size), "The size of the font [12]. Can be between 6 and 20.")
          ("output,o", po::value< std::string >(&output_path), "Where to store the output images [data].")
          ("version,V", "Print the version number.")
          ("verbose,v", po::bool_switch(&verbose), "Print more verbose output during processing.")
          ("input,i", po::value< std::string >(&story_file), "A story json file (array of array of strings).")
        ;
    po::positional_options_description p;
    p.add("input", -1);

    po::variables_map vm;
    try {
        po::store(po::command_line_parser(argc, argv).
                options(desc).positional(p).run(), vm);
        po::notify(vm);
    } catch(std::exception& e) {
            std::cout << e.what() << "\n";
            return 1;
    }

    if (vm.count("help") || argc <= 1) {
        std::cout << desc << std::endl;
        return 0;
    }

    std::string versionString = std::string("0.0.1.") + boost::replace_all_copy(std::string(__DATE__), " ", ".");
    if (vm.count("version")) {
        fprintf(stdout, "version: %s\n", versionString.c_str());
        return 0;
    }

    if (font_size >= HEIGHT) {
      fprintf(stderr, "Warning: Using a larger than 28 font size might result in broken letters, re-compile with a larger HEIGHT value.\n");
    }

    std::string output = output_path;

    //int font_size = 12;
    int face_index;

    // we should parse the stories file
    bool configFileExists = false;
    std::string dn = story_file;
    struct stat stat_buffer;
    if (!(stat(dn.c_str(), &stat_buffer) == 0)) {
        configFileExists = false;
    } else {
        configFileExists = true;
        if (verbose)
            fprintf(stdout, "  Found config file in: %s\n", story_file.c_str());
    }
    std::vector<std::vector<std::string>> stories;
    if (configFileExists) {
        std::ifstream f(story_file);
        json data = json::parse(f);

        // pt::read_json(story_file.c_str(), root);
        int numStories = data.size(); // root.get_value<std::vector<std::vector<std::string>>>().size();
        stories = data;
        if (verbose)
          fprintf(stdout, "found %d stories\n", numStories);
    }

    dn = font_path;
    struct stat buf;
    if (!(stat(dn.c_str(), &buf) == 0)) {
        fprintf(stderr, "Error: no font found.\n");
        exit(-1);
    }

    // and start
    FT_Library library;

    double angle;
    int target_height;
    int n, num_chars;

    int font_length = 20;
    float current_image_max_value = 255;
    float current_image_min_value = 0;

    for (int i = 0; i < stories.size(); i++) {  // how many images
        if (verbose) {
            fprintf(stdout, "[ create file %d/%zu ]\r", i + 1, stories.size());
            fflush(stdout);
        }

        face_index = 0;
        const char *filename = font_path.c_str(); /* first argument     */

        FT_Face face;
        FT_GlyphSlot slot;
        FT_Matrix matrix; /* transformation matrix */
        FT_Vector pen;    /* untransformed origin  */
        FT_Error error;

        // the character data is written into image
        memset(image_buffer, 0, HEIGHT * WIDTH);

        unsigned short xmax;
        unsigned short ymax;

        xmax = 1024;  // (unsigned short)extent[0];
        ymax = 768;  // (unsigned short)extent[1];

        // fprintf(stdout, "Found buffer of length: %ld\n", len);
        int len = 1024 * 768;
        char *buffer = new char[len*sizeof(unsigned short)];
        // initialize the background of the image
        memset(buffer, 0, sizeof(unsigned short)*len);

        dn = output;
        if (!(stat(dn.c_str(), &buf) == 0)) {
            mkdir(dn.c_str(), 0777);
        }

        if (!(stat(dn.c_str(), &buf) == 0)) {
            mkdir(dn.c_str(), 0777);
        }

        char outputfilename[1024];

        snprintf(outputfilename, 1024 - 1, "%s/%08d.png", output.c_str(), i);

        float pmin = 0;    // current_image_min_value;
        float pmax = 255;  // current_image_max_value;
        int bitsAllocated = 16;
        std::vector<float> color_background_size;
        std::vector<float> color_background_color;
        std::vector<float> color_pen_color;
        float vary_percent;

        { // color setting by placement
            color_background_size = {255, 255, 255, 0};   // colors[idx][0], colors[idx][1], colors[idx][2], colors[idx][3]};
            color_background_color = {1, 1, 1, 1};  // colors[idx][4], colors[idx][5], colors[idx][6], colors[idx][7]};
            color_pen_color = {1, 1, 1, 1};   // colors[idx][8], colors[idx][9], colors[idx][10], colors[idx][11]};
            vary_percent = 0.3;                     // colors[idx][12];
        }
        float vx_min = .1;  // placements[placement]["x"][0];
        float vx_max = .1;  // placements[placement]["x"][1];
        float vx = vx_min;  //  + ((rand() * 1.0f) / (1.0f * RAND_MAX)) * (vx_max - vx_min);
        float vy_min = .1;  // placements[placement]["y"][0];
        float vy_max = .1;  // placements[placement]["y"][1];
        float vy = vy_min;  //  + ((rand() * 1.0f) / (1.0f * RAND_MAX)) * (vy_max - vy_min);
        int start_px, start_py;
        bool neg_x = false;
        bool neg_y = false;
        if (vx >= 0) {
            start_px = std::floor(xmax * vx);
        } else {
            start_px = xmax - std::floor(xmax * -vx);
            neg_x = true;
        }
        if (vy >= 0) {
            start_py = std::floor(ymax * vy);
        } else {
            start_py = ymax - std::floor(ymax * -vy);
            neg_y = true;
        }
        int howmany = stories[i].size();  // how many lines do we have
        //memset(buffer, 0, 1024 * 1024);

        for (int text_lines = 0; text_lines < howmany; text_lines++) {
            memset(image_buffer, 0, HEIGHT * WIDTH);

            float repeat_spacing = 1.2;
            int px = start_px;
            int py = start_py + (neg_y ? -1 : 1) * (text_lines * font_size +
                                                    text_lines * (repeat_spacing * 0.5 * font_size));

            font_length = 30;  // (rand() % (lengths_max - lengths_min)) + lengths_min;
            std::string text2printSTD = stories[i][text_lines];
            num_chars = text2printSTD.size();
            angle = 0;  // ( 25.0 / 360 ) * 3.14159 * 2;      /* use 25 degrees     */
            target_height = HEIGHT;

            error = FT_Init_FreeType(&library); /* initialize library */
            if (error != 0) {
                fprintf(stderr,
                        "Error: The freetype library could not be initialized with this font.\n");
                exit(-1);
            }

            error = FT_New_Face(library, filename, face_index, &face); /* create face object */
            if (face == NULL) {
                fprintf(stderr, "Error: no face found, provide the filename of a ttf file...\n");
                exit(-1);
            }
            float font_size_in_pixel = font_size;
            error = FT_Set_Char_Size(face, font_size_in_pixel * 64, 0, 96, 0); /* set character size */
            if (error != 0) {
                // try with the next font size
                fprintf(stderr, "Warning: FT_Set_Char_Size returned error for size %d.\n", font_size);
                continue;
            }

            slot = face->glyph;

            /* set up matrix */
            matrix.xx = (FT_Fixed)(cos(angle) * 0x10000L);
            matrix.xy = (FT_Fixed)(-sin(angle) * 0x10000L);
            matrix.yx = (FT_Fixed)(sin(angle) * 0x10000L);
            matrix.yy = (FT_Fixed)(cos(angle) * 0x10000L);

            /* the pen position in 26.6 cartesian space coordinates; */
            /* start at (300,200) relative to the upper left corner  */
            pen.x = 1 * 64;
            pen.y = (target_height - 20) * 64;

            for (n = 0; n < num_chars; n++) {
                /* set transformation */
                FT_Set_Transform(face, &matrix, &pen);

                error = FT_Load_Char(face, text2printSTD[n], FT_LOAD_RENDER);
                if (error)
                    continue; /* ignore errors */

                /* now, draw to our target surface (convert position) */
                draw_bitmap(&slot->bitmap, slot->bitmap_left, target_height - slot->bitmap_top);

                /* increment pen position */
                pen.x += slot->advance.x;
                pen.y += slot->advance.y;
            }

            FT_Done_Face(face);

            // draw the text
            bool leaveWithoutText = false;
            if (!leaveWithoutText) {
                if (bitsAllocated == 16) {
                    signed short *bvals = (signed short *)buffer;
                    for (int yi = 0; yi < HEIGHT; yi++) {
                        for (int xi = 0; xi < WIDTH; xi++) {
                            if (image_buffer[yi][xi] == 0)
                                continue;
                            // I would like to copy the value from image over to
                            // the buffer. At some good location...
                            int newx = px + xi;
                            int newy = py + yi;
                            int idx = newy * xmax + newx;
                            if (newx < 0 || newx >= xmax || newy < 0 || newy >= ymax)
                                continue;
                            if (image_buffer[yi][xi] == 0)
                                continue;

                            // instead of blending we need to use a fixed overlay color
                            // we have image information between current_image_min_value and current_image_max_value
                            // we need to scale the image_buffer by those values.
                            float f = 0;
                            // float v = (f * bvals[idx]) + ((1.0 - f) * ((1.0 * image_buffer[yi][xi]) / 512.0 * current_image_max_value));
                            float v = (1.0f * image_buffer[yi][xi] / 255.0);  // 0 to 1 for color, could be inverted if we have a white background
                            // clamp to 0 to 1
                            v = std::max(0.0f, std::min(1.0f, v));
                            float w = 1.0f * bvals[idx] / current_image_max_value;
                            float alpha_blend = (v + w * (1.0f - v));
                            // should be random value now not exceeding (vary_percent/100)
                            if (color_pen_color[0] == 0) {  // instead of white on black do now black on white
                                alpha_blend = 1.0f - v;
                            }

                            // fprintf(stdout, "%d %d: %d\n", xi, yi, bvals[idx]);
                            bvals[idx] = (signed short)std::max(
                                0.0f, std::min(current_image_max_value, current_image_min_value + (alpha_blend) * (current_image_max_value - current_image_min_value)));
                            // fprintf(stdout, "%d %d: %d\n", xi, yi, bvals[idx]);
                        }
                    }
                }
            }
        }
        // write out the image as a png, 16 bit with transparency
        {
            snprintf(outputfilename, 1024 - 1, "%s/%08d.png", output.c_str(), i);
            rgba16_image_t img(xmax, ymax);
            //rgb16_pixel_t red(65535, 0, 0);
            // we should copy the values over now --- from the buffer? Or from the DICOM Image
            //fill_pixels(view(img), red);
            // stretch the intensities from 0 to max for png (0...65535)
            float pmin = current_image_min_value;
            float pmax = current_image_max_value;
            auto v = view(img);
            auto it = v.begin();
            if (bitsAllocated == 16) {
                signed short *bvals = (signed short *)buffer;
                while (it != v.end()) {
                    //++hist[*it];
                    float pixel_val = ((1.0 * bvals[0]) - pmin) / (pmax - pmin); // 0 to 1, with 0 background transparent and 1 foreground opaque
                    // Alpha-blend two values, background and foreground.
                    float alpha_a = 1.0f - pixel_val;
                    float alpha_b = 0.9f; // background is yet fully transparent, why do we need this?
                    float alpha_over = alpha_a + alpha_b *(1.0f - alpha_a);
                    float Ca[3] = { 1, 1, 1};
                    float Cb[3] = { 0, 0, 0};
                    float Cr[4] = { 0, 0, 0, alpha_over};
                    Cr[0] = ((Ca[0] * alpha_a) + (Cb[0] * alpha_b * (1.0f - alpha_a)) ) / alpha_over;
                    Cr[1] = ((Ca[1] * alpha_a) + (Cb[1] * alpha_b * (1.0f - alpha_a)) ) / alpha_over;
                    Cr[2] = ((Ca[2] * alpha_a) + (Cb[2] * alpha_b * (1.0f - alpha_a)) ) / alpha_over;

                    //*it = rgb16_pixel_t{(short unsigned int)(pixel_val * 65535), (short unsigned int)(pixel_val * 65535), (short unsigned int)(pixel_val * 65535)};
                    *it = rgba16_pixel_t{(short unsigned int)(Cr[0] * 65535), (short unsigned int)(Cr[1] * 65535), (short unsigned int)(Cr[2] * 65535), (short unsigned int)(alpha_over * 65535)};
                    bvals++;
                    it++;
                }
                write_view(outputfilename, const_view(img), png_tag{});
            }
        }
        delete[] buffer;
        // delete im; (done using smart pointers)
    }
    FT_Done_FreeType(library);

    return 0;
}
