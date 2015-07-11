#include <stdio.h>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>

SOCKET server_socket;

struct HTTP_Status_Description {
	int status_code;
	const char *description;
} http_status_descriptions[] = {
	{ 200, "OK" },
	{ 404, "Not found" },
};

const char *get_http_status_description(int status_code) {
	for (U32 i = 0; i < Count(http_status_descriptions); i++) {
		if (http_status_descriptions[i].status_code == status_code) {
			return http_status_descriptions[i].description;
		}
	}
	return "Unknown";
}

void handle_kill(int signal)
{
	puts("server is kill");

	closesocket(server_socket);
	
	WSACleanup();
	exit(0);
}

struct WorldInstance
{
	World *world;
	CRITICAL_SECTION lock;
	time_t last_updated;
};

void update_to_now(WorldInstance *world_instance)
{
	LARGE_INTEGER begin, end, freq;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&begin);

	int count = 0;
	time_t now = time(NULL);
	while (world_instance->last_updated < now) {
		count++;
		world_tick(world_instance->world);
		world_instance->last_updated++;
	}
	QueryPerformanceCounter(&end);
	I64 diff = end.QuadPart - begin.QuadPart;
	I64 ticks = diff * 100000LL / freq.QuadPart;
	float ms = (float)ticks / 100.0f;

	printf("Updated world %d ticks: Took %.2fms\n", count, ms);
}

DWORD WINAPI UpdateThread(void *world_instance_ptr)
{
	WorldInstance *world_instance = (WorldInstance*)world_instance_ptr;

	for (;;) {
		EnterCriticalSection(&world_instance->lock);
		update_to_now(world_instance);
		LeaveCriticalSection(&world_instance->lock);
		Sleep(10000);
	}
}

int main(int argc, char **argv)
{
	WSADATA wsadata;
	WSAStartup(0x0202, &wsadata);

	signal(SIGINT, handle_kill);

	struct addrinfo *addr = NULL;
	struct addrinfo hints = { 0 };
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, "3500", &hints, &addr);

	server_socket = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	if (bind(server_socket, addr->ai_addr, (int)addr->ai_addrlen) == SOCKET_ERROR) {
		printf("bind failed: %d", WSAGetLastError());
	}
	if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
		printf("listen failed: %d", WSAGetLastError());
	}

	freeaddrinfo(addr);

	const char *names[] = {
		"Urist", "Gimli", "Thir", "Tharun", "Dofor", "Ufir",
		"Bohir",
	};

	char name_buf[512], *name_ptr = name_buf;

	static World world = { 0 };

	for (U32 id = 1; id < 10; id++) {
		char *name = name_ptr;
		name_ptr += 1 + sprintf(name_ptr, "%s %sson",
			names[rand() % Count(names)], names[rand() % Count(names)]);

		Dwarf *dwarf = &world.dwarves[id];
		dwarf->id = id;
		dwarf->name = name;
		dwarf->hunger = rand() % 50;
		dwarf->sleep = rand() % 50;
	}

	WorldInstance world_instance = { 0 };
	world_instance.last_updated = time(NULL);
	world_instance.world = &world;
	InitializeCriticalSection(&world_instance.lock);

	CreateThread(NULL, NULL, UpdateThread, &world_instance, NULL, NULL);

	for (;;) {
		SOCKET client_socket = accept(server_socket, NULL, NULL);
		if (client_socket == INVALID_SOCKET)
			continue;

		char buffer[512];
		int bytes_read = recv(client_socket, buffer, sizeof(buffer), 0);

		char method[64];
		char path[128];
		char http_version[32];
		sscanf(buffer, "%s %s %s\r\n", method, path, http_version);

		printf("Request to path %s\n", path);

		U32 id;

		if (!strcmp(path, "/favicon.ico")) {
			puts("Serving favicon");

			FILE *icon = fopen("data/icon.ico", "rb");
			fseek(icon, 0, SEEK_END);
			int size = ftell(icon);
			fseek(icon, 0, SEEK_SET);

			const char *response_start = "HTTP/1.1 200 OK\r\n";
			char content_length[128];
			sprintf(content_length, "Content-Length: %d\r\n", size);
			const char *content_type = "Content-Type: image/x-icon\r\n";
			const char *separator = "\r\n";

			send(client_socket, response_start, (int)strlen(response_start), 0);
			send(client_socket, content_length, (int)strlen(content_length), 0);
			send(client_socket, content_type, (int)strlen(content_type), 0);
			send(client_socket, separator, (int)strlen(separator), 0);

			while (!feof(icon)) {
				char iconbuf[512];
				int num = (int)fread(iconbuf, 1, sizeof(iconbuf), icon);

				send(client_socket, iconbuf, num, 0);
			}

			fclose(icon);

		} else if (!strcmp(path, "/dwarves")) {
			char body[1024];

			EnterCriticalSection(&world_instance.lock);
			update_to_now(&world_instance);
			int status = render_dwarves(world_instance.world, body);
			LeaveCriticalSection(&world_instance.lock);

			const char *status_desc = get_http_status_description(status);
			char response_start[128];
			sprintf(response_start, "HTTP/1.1 %d %s\r\n", status, status_desc);

			char content_length[128];
			sprintf(content_length, "Content-Length: %d\r\n", strlen(body));
			const char *separator = "\r\n";

			send(client_socket, response_start, (int)strlen(response_start), 0);
			send(client_socket, content_length, (int)strlen(content_length), 0);
			send(client_socket, separator, (int)strlen(separator), 0);
			send(client_socket, body, (int)strlen(body), 0);

		} else if (!strcmp(path, "/feed")) {
			char body[1024];

			EnterCriticalSection(&world_instance.lock);
			update_to_now(&world_instance);
			int status = render_feed(world_instance.world, body);
			LeaveCriticalSection(&world_instance.lock);

			const char *status_desc = get_http_status_description(status);
			char response_start[128];
			sprintf(response_start, "HTTP/1.1 %d %s\r\n", status, status_desc);

			char content_length[128];
			sprintf(content_length, "Content-Length: %d\r\n", strlen(body));
			const char *separator = "\r\n";

			send(client_socket, response_start, (int)strlen(response_start), 0);
			send(client_socket, content_length, (int)strlen(content_length), 0);
			send(client_socket, separator, (int)strlen(separator), 0);
			send(client_socket, body, (int)strlen(body), 0);
		
		} else if (sscanf(path, "/entities/%d", &id) == 1) {
			char body[1024];

			EnterCriticalSection(&world_instance.lock);
			update_to_now(&world_instance);
			int status = render_entity(world_instance.world, id, body);
			LeaveCriticalSection(&world_instance.lock);

			const char *status_desc = get_http_status_description(status);
			char response_start[128];
			sprintf(response_start, "HTTP/1.1 %d %s\r\n", status, status_desc);

			char content_length[128];
			sprintf(content_length, "Content-Length: %d\r\n", strlen(body));
			const char *separator = "\r\n";

			send(client_socket, response_start, (int)strlen(response_start), 0);
			send(client_socket, content_length, (int)strlen(content_length), 0);
			send(client_socket, separator, (int)strlen(separator), 0);
			send(client_socket, body, (int)strlen(body), 0);

		} else {
			const char *body = "<html><body><h1>Hello world!</h1></body></html>";

			const char *response_start = "HTTP/1.1 200 OK\r\n";
			char content_length[128];
			sprintf(content_length, "Content-Length: %d\r\n", strlen(body));
			const char *separator = "\r\n";

			send(client_socket, response_start, (int)strlen(response_start), 0);
			send(client_socket, content_length, (int)strlen(content_length), 0);
			send(client_socket, separator, (int)strlen(separator), 0);
			send(client_socket, body, (int)strlen(body), 0);
		}

		closesocket(client_socket);
	}
}

