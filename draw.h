void drawBox(uint32_t* field, int dimX, int dimY, int x, int y, int w, int h, uint32_t color);

void drawText(uint32_t* field, int dimX, int dimY, const unsigned char* fontData, char* string, int size, int x, int y, uint32_t color);

void drawCircle(uint32_t* field, int dimX, int dimY, int x, int y, int r, uint32_t color);

void drawLine(uint32_t* field, int dimX, int dimY, int x0, int y0, int x1, int y1, uint32_t color);
