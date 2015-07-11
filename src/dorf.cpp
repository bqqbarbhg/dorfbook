#include <stdio.h>
#include <stdint.h>

typedef int32_t I32;
typedef uint32_t U32;
#define Count(array) (sizeof(array)/sizeof(*(array)))

struct Dwarf
{
	U32 id;
	const char *name;
	I32 hunger;
	I32 sleep;
};

struct World
{
	Dwarf dwarves[64];
};

int render_entity(World *world, U32 id, char *buffer)
{
	char *ptr = buffer;
	Dwarf *dwarf = 0;
	for (U32 i = 0; i < Count(world->dwarves); i++) {
		if (world->dwarves[i].id == id) {
			dwarf = &world->dwarves[i];
			break;
		}
	}

	if (!dwarf) {
		sprintf(buffer, "Entity not found with ID #%u", id);
		return 404;
	}

	ptr += sprintf(ptr, "<html><head><title>%s</title></head>", dwarf->name);
	ptr += sprintf(ptr, "<body><h1>%s</h1>", dwarf->name); 
	ptr += sprintf(ptr, "<h2>Hunger: %d, sleep: %d</h2>", dwarf->hunger, dwarf->sleep); 
	ptr += sprintf(ptr, "</body></html>"); 

	return 200;
}

