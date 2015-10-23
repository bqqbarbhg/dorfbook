#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>

#define DORF_PORT "3500"

os_socket server_socket;
os_atomic_uint32 active_thread_count;

struct Server_Stats
{
	U32 snapshot_count;
	U32 snapshot_index;
	long *active_thread_counts;
	os_mutex lock;
};

Server_Stats global_stats;

struct HTTP_Status_Description {
	int status_code;
	const char *description;
} http_status_descriptions[] = {
	/* 1xx Informational */
	{ 100, "Continue" },
	{ 101, "Switching Protocols" },
	{ 102, "Processing" },

	/* 2xx Success */
	{ 200, "OK" },
	{ 201, "Created" },
	{ 202, "Accepted" },
	{ 203, "Non-Authoritative Information" },
	{ 204, "No Content" },
	{ 205, "Reset Content" },
	{ 206, "Partial Content" },
	{ 207, "Multi-Status" },
	{ 208, "Already Reported" },
	{ 226, "IM Used" },

	/* 3xx Redirection */
	{ 300, "Multiple Choices" },
	{ 301, "Moved Permanently" },
	{ 302, "Found" },
	{ 303, "See Other" },
	{ 304, "Not Modified" },
	{ 305, "Use Proxy" },
	/* { 306, "Switch Proxy" }, */
	{ 307, "Temporary Redirect" },
	{ 308, "Permanent Redirect" },

	/* 4xx Client Error */
	{ 400, "Bad Request" },
	{ 401, "Unauthorized" },
	{ 402, "Payment Required" },
	{ 403, "Forbidden" },
	{ 404, "Not Found" },
	{ 405, "Method Not Allowed" },
	{ 406, "Not Acceptable" },
	{ 407, "Proxy Authentication Required" },
	{ 408, "Request Timeout" },
	{ 409, "Conflict" },
	{ 410, "Gone" },
	{ 411, "Length Required" },
	{ 412, "Precondition Failed" },
	{ 413, "Payload Too Large" },
	{ 414, "Request-URI Too Long" },
	{ 415, "Unsupported Media Type" },
	{ 416, "Requested Range Not Satisfiable" },
	{ 417, "Expectation Failed" },
	{ 418, "I'm a teapot" },
	{ 420, "Blaze it" },

	/* 5xx Server Error */
	{ 500, "Internal Server Error" },
	{ 501, "Not Implemented" },
	{ 502, "Bad Gateway" },
	{ 503, "Service Unavailable" },
	{ 504, "Gateway Timeout" },
	{ 505, "HTTP Version Not Supported" },
	{ 506, "Variant Also Negotiates" },
	{ 507, "Insufficient Storage" },
	{ 508, "Loop Detected" },
	/* { 509, "Bandwidth Limit Exceeded" }, */
	{ 510, "Not Extended" },
	{ 511, "Network Authentication Required" },
	/* { 520, "Unknown Error" }, */
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

	os_socket_close(server_socket);

	exit(0);
}

struct World_Instance
{
	World *world;
	os_mutex lock;
	time_t last_updated;
};

void update_to_now(World_Instance *world_instance)
{
	os_timer_mark begin = os_get_timer();

	int count = 0;
	time_t now = time(NULL);
	while (world_instance->last_updated < now) {
		count++;
		world_tick(world_instance->world);
		world_instance->last_updated++;
	}

	os_timer_mark end = os_get_timer();

	float ms = os_timer_delta_ms(begin, end);
	if (count > 0)
		printf("Updated world %d ticks: Took %.2fms\n", count, ms);
}

OS_THREAD_ENTRY(thread_background_world_update, world_instance_ptr)
{
	World_Instance *world_instance = (World_Instance*)world_instance_ptr;

	for (;;) {
		os_mutex_lock(&world_instance->lock);
		update_to_now(world_instance);
		os_mutex_unlock(&world_instance->lock);
		os_sleep_seconds(10);
	}
}

OS_THREAD_ENTRY(thread_background_stat_update, server_stats)
{
	Server_Stats *stats = (Server_Stats*)server_stats;
	for (;;) {

		os_mutex_lock(&stats->lock);
		stats->active_thread_counts[stats->snapshot_index] = active_thread_count;

		stats->snapshot_index = (stats->snapshot_index + 1) % stats->snapshot_count;
		os_mutex_unlock(&stats->lock);

		os_sleep_seconds(1);
	}
}

int render_stats(Server_Stats *stats, char *body)
{
	char *ptr = body;

	ptr += sprintf(ptr, "<html><head><title>Server stats</title></head><body>");
	ptr += sprintf(ptr, "<h5>Active thread count</h5>");
	ptr += sprintf(ptr, "<svg width=\"400\" height=\"200\">\n");

	long max_thread_count = 1;
	for (U32 i = 0; i < stats->snapshot_count; i++) {
		max_thread_count = max(max_thread_count, stats->active_thread_counts[i]);
	}

	long ruler_size = (long)ceilf((float)max_thread_count / 5);
	long ruler_count = max_thread_count / ruler_size + 1;
	long graph_height = ruler_count * ruler_size;
	for (long i = 0; i <= ruler_count; i++) {
		long value = i * ruler_size;
		float y = 195.0f - (float)value / graph_height * 170.0f;
		ptr += sprintf(ptr, "<path d=\"M30 %f L400 %f\" stroke=\"#ddd\" stroke-width=\"1\""
			" fill=\"none\" />\n", y, y);
		ptr += sprintf(ptr, "<text x=\"25\" y=\"%f\" text-anchor=\"end\" "
			"fill=\"gray\">%d</text>", y + 4.0f, (int)value);
	}

	ptr += sprintf(ptr, "<path d=\"");
	char command_char = 'M';
	for (U32 i = 0; i < stats->snapshot_count; i++) {
		int snapshot_index = (stats->snapshot_index - 1 - i + stats->snapshot_count)
			% stats->snapshot_count;

		float x = 400.0f - ((float)i / (stats->snapshot_count - 1)) * 370.0f;
		float y = 195.0f - (float)stats->active_thread_counts[snapshot_index]
			/ graph_height * 170.0f;

		ptr += sprintf(ptr, "%c%f %f ", command_char, x, y);
		command_char = 'L';
	}
	ptr += sprintf(ptr, "\" stroke=\"black\" stroke-width=\"2\" fill=\"none\" />\n");
	ptr += sprintf(ptr, "</svg>");

	return 200;
}

int render_heap(char *body)
{
	char *ptr = body;

#if BUILD_DEBUG

	ptr += sprintf(ptr, "<html><head><title>Server heap</title></head><body>");
	ptr += sprintf(ptr, "<table>");

	Debug_Alloc_Header *header = debug_alloc_lock_heap();
	for (; header; header = header->next) {
		ptr += sprintf(ptr, "<a href=\"allocations/%llu\">", header->serial);
		ptr += sprintf(ptr, "<tr><td>%s:%d</td>"
				"<td><a href=\"allocations/%llu\"><pre>%s[%d]</pre></a></td>"
				"<td>%.2fkB</td></tr>",
				header->alloc_loc.file,
				header->alloc_loc.line,
				header->serial,
				header->type,
				(U32)header->size / (U32)header->type_size,
				(double)header->size / 1000.0);
	}

	debug_alloc_unlock_heap();

	ptr += sprintf(ptr, "</table>");
	ptr += sprintf(ptr, "</body></html>");

	return 200;
#else
	return 404;
#endif
}

int render_allocations(char *body)
{
	char *ptr = body;

#if BUILD_DEBUG

	ptr += sprintf(ptr, "<html><head><title>Server allocations</title></head><body>");
	ptr += sprintf(ptr, "<table>");

	Debug_Alloc_Header header;

	U64 serial = g_debug_memory.serial;
	for (; debug_alloc_get_serial(serial, &header); serial--) {

		ptr += sprintf(ptr, "<a href=\"allocations/%llu\">", header.serial);
		ptr += sprintf(ptr, "<tr><td>%s:%d</td>"
				"<td><a href=\"allocations/%llu\"><pre>%s[%d]</pre></a></td>"
				"<td>%.2fkB</td></tr>",
				header.alloc_loc.file,
				header.alloc_loc.line,
				header.serial,
				header.type,
				(U32)header.size / (U32)header.type_size,
				(double)header.size / 1000.0);
	}


	ptr += sprintf(ptr, "</table>");
	ptr += sprintf(ptr, "</body></html>");

	return 200;
#else
	return 404;
#endif
}

int render_allocation(char *body, U64 serial)
{
	char *ptr = body;

#if BUILD_DEBUG

	Debug_Alloc_Header header;
	if (!debug_alloc_get_serial(serial, &header)) {
		return 404;
	}

	ptr += sprintf(ptr, "<html><head><title>Server allocations</title></head><body>");

	ptr += sprintf(ptr, "<h2>%s[%d] (%.2fkB)</h2>",
			header.type,
			header.size / header.type_size,
			(double)header.size / 1000.0f);

	if (header.next_serial) {
		Debug_Alloc_Header new_header;

		if (debug_alloc_get_serial(header.next_serial, &new_header)) {
			ptr += sprintf(ptr, "<h3>Reallocated as <a href=\"/allocations/%llu\">%s[%d] (%.2fkB)</a></h3>",
					new_header.serial,
					new_header.type,
					new_header.size / new_header.type_size,
					(double)new_header.size / 1000.0f);
		} else {
			ptr += sprintf(ptr, "<h3>Reallocated</h3>");
		}
	}

	if (header.prev_serial) {
		Debug_Alloc_Header new_header;

		if (debug_alloc_get_serial(header.prev_serial, &new_header)) {
			ptr += sprintf(ptr, "<h3>Reallocated from <a href=\"/allocations/%llu\">%s[%d] (%.2fkB)</a></h3>",
					new_header.serial,
					new_header.type,
					new_header.size / new_header.type_size,
					(double)new_header.size / 1000.0f);
		} else {
			ptr += sprintf(ptr, "<h3>Reallocated</h3>");
		}
	}

	if (header.alloc_trace_length) {
		os_symbol_info *symbols = os_get_address_infos(header.alloc_trace, (int)header.alloc_trace_length);
		if (symbols) {
			ptr += sprintf(ptr, "<h4>Allocation trace</h4><table>\n");
			for (U32 i = 0; i < header.alloc_trace_length; i++) {
				if (!symbols[i].filename || !symbols[i].function)
					continue;
				ptr += sprintf(ptr, "<tr><td>%s:%d</td><td>%s</td></tr>\n",
					symbols[i].filename, symbols[i].line, symbols[i].function);
			}
			ptr += sprintf(ptr, "</table>\n");
		}
	}

	if (header.free_trace_length) {
		os_symbol_info *symbols = os_get_address_infos(header.free_trace, (int)header.free_trace_length);
		if (symbols) {
			ptr += sprintf(ptr, "<h4>Free trace</h4><table>\n");
			for (U32 i = 0; i < header.free_trace_length; i++) {
				if (!symbols[i].filename || !symbols[i].function)
					continue;
				ptr += sprintf(ptr, "<tr><td>%s:%d</td><td>%s</td></tr>\n",
					symbols[i].filename, symbols[i].line, symbols[i].function);
			}
			ptr += sprintf(ptr, "</table>\n");
		}
	}

	ptr += sprintf(ptr, "</body></html>");

	return 200;
#else
	return 404;
#endif
}

struct Response_Thread_Data
{
	os_socket client_socket;
	World_Instance *world_instance;
	char *body_storage;
	int thread_id;
};

struct Socket_Buffer
{
	os_socket socket;
	char *data;
	int data_size;
	int pos;
	int size;
	int limit_left;
};

struct Read_Block
{
	char *data;
	int length;
};

Socket_Buffer buffer_new(os_socket socket, int size=1024)
{
	Socket_Buffer buffer = { 0 };
	buffer.socket = socket;
	buffer.data = M_ALLOC(char, size);
	buffer.data_size = size;
	return buffer;
}

void buffer_limit(Socket_Buffer *buffer, int bytes)
{
	buffer->limit_left = bytes;
}

// NOTE: Does not free the socket
void buffer_free(Socket_Buffer *buffer)
{
	M_FREE(buffer->data);
}

bool buffer_fill_read(Socket_Buffer *buffer)
{
	int to_read = min(buffer->data_size, buffer->limit_left);
	if (to_read <= 0)
		return false;

	int bytes_read = recv(buffer->socket, buffer->data, to_read, 0);
	buffer->limit_left -= to_read;
	buffer->size = bytes_read;
	buffer->pos = 0;
	return bytes_read > 0;
}

bool buffer_peek(Socket_Buffer *buffer, Read_Block *block, int length)
{
	int buffer_left = buffer->size - buffer->pos;
	if (buffer_left == 0) {
		if (!buffer_fill_read(buffer))
			return false;
		buffer_left = buffer->size - buffer->pos;
	}

	block->data = buffer->data + buffer->pos;
	block->length = min(length, buffer_left);
	return true;
}

bool buffer_read(Socket_Buffer *buffer, Read_Block *block, int length)
{
	if (!buffer_peek(buffer, block, length))
		return false;
	buffer->pos += block->length;
	return true;
}

bool buffer_accept(Socket_Buffer *buffer, const char *value, int length)
{
	int pos = 0;
	Read_Block block;
	while (length - pos > 0) {
		if (!buffer_read(buffer, &block, length - pos))
			return false;
		if (memcmp(block.data, value + pos, block.length))
			return false;
		pos += block.length;
	}
	return true;
}

int buffer_read_line(Socket_Buffer *buffer, char *line, int length)
{
	int pos = 0;
	Read_Block block;
	while (length - pos > 0) {
		if (!buffer_peek(buffer, &block, length - pos)) return -3;

		char *end = (char*)memchr(block.data, '\r', block.length);
		int in_length = end ? (int)(end - block.data) : block.length;

		// Check for buffer overflow and null-byte injection
		if (in_length > length - pos - 1) return -1;
		if (memchr(block.data, '\0', in_length)) return -4;

		// Copy until block end or \r
		memcpy(line + pos, block.data, in_length);
		buffer->pos += in_length;
		pos += in_length;

		if (end) {
			if (!buffer_accept(buffer, "\r\n", 2)) return -2;
			line[pos] = '\0';
			return pos;
		}
	}

	// Reached the end of the line buffer without finding a \r
	return -1;
}

int buffer_read_amount(Socket_Buffer *buffer, char *data, int length)
{
	int pos = 0;
	Read_Block block;
	while (length - pos > 0) {
		if (!buffer_read(buffer, &block, length - pos))
			return false;
		memcpy(data + pos, block.data, block.length);
		pos += block.length;
	}
	return true;
}

void send_response(os_socket socket, const char *content_type, int status,
	const char *body, size_t body_length, String *extra_headers, U32 extra_header_count)
{
	const char *status_desc = get_http_status_description(status);
	char response_start[128];
	sprintf(response_start, "HTTP/1.1 %d %s\r\n", status, status_desc);

	char content_length_header[128];
	sprintf(content_length_header, "Content-Length: %d\r\n", (int)body_length);
	char content_type_header[128];
	sprintf(content_type_header, "Content-Type: %s\r\n", content_type);
	const char *separator = "\r\n";

	os_socket_send(socket, response_start, (int)strlen(response_start));
	os_socket_send(socket, content_length_header, (int)strlen(content_length_header));
	os_socket_send(socket, content_type_header, (int)strlen(content_type_header));

	for (U32 i = 0; i < extra_header_count; i++) {
		String header = extra_headers[i];
		os_socket_send(socket, header.data, (int)header.length);
		os_socket_send(socket, separator, (int)strlen(separator));
	}

	os_socket_send(socket, separator, (int)strlen(separator));
	os_socket_send_and_flush(socket, body, (int)body_length);
}

void send_response(os_socket socket, const char *content_type, int status,
	const char *body, size_t body_length)
{
	send_response(socket, content_type, status, body, body_length, 0, 0);
}

void send_text_response(os_socket socket, const char *content_type, int status,
	const char *body)
{
	send_response(socket, content_type, status, body, strlen(body));
}

OS_THREAD_ENTRY(thread_do_response, thread_data)
{
	Response_Thread_Data *data = (Response_Thread_Data*)thread_data;
	os_socket client_socket = data->client_socket;
	World_Instance *world_instance = data->world_instance;
	char *body = data->body_storage;

	os_atomic_increment(&active_thread_count);

	Socket_Buffer buffer = buffer_new(client_socket);

	for (;;) {

		// Allow only 8kB of request line and headers, but reset on every request
		buffer_limit(&buffer, KB(8));

		char line[256];
		if (buffer_read_line(&buffer, line, sizeof(line)) < 0)
			break;

		os_timer_mark begin_respond = os_get_timer();

		char method[64];
		char path[128];
		char http_version[32];
		sscanf(line, "%s %s %s\r\n", method, path, http_version);

		unsigned int content_length = 0;

		bool failed = false;
		while (strlen(line)) {
			if (buffer_read_line(&buffer, line, sizeof(line)) < 0) {
				failed = true;
				break;
			}
			// TODO: Case-insensitive headers
			sscanf(line, "Content-Length: %d", &content_length);
		}
		if (failed)
			break;

		U32 id;
		U64 serial;
		char test_name[128];
		if (!strcmp(path, "/favicon.ico")) {
			FILE *icon = fopen("data/icon.ico", "rb");
			fseek(icon, 0, SEEK_END);
			int size = ftell(icon);
			fseek(icon, 0, SEEK_SET);

			const char *response_start = "HTTP/1.1 200 OK\r\n";
			char content_length[128];
			sprintf(content_length, "Content-Length: %d\r\n", size);
			const char *content_type = "Content-Type: image/x-icon\r\n";
			const char *separator = "\r\n";

			os_socket_send(client_socket, response_start, (int)strlen(response_start));
			os_socket_send(client_socket, content_length, (int)strlen(content_length));
			os_socket_send(client_socket, content_type, (int)strlen(content_type));
			os_socket_send(client_socket, separator, (int)strlen(separator));

			while (!feof(icon)) {
				char iconbuf[512];
				int num = (int)fread(iconbuf, 1, sizeof(iconbuf), icon);

				if (feof(icon)) {
					os_socket_send_and_flush(client_socket, iconbuf, num);
				} else {
					os_socket_send(client_socket, iconbuf, num);
				}
			}

			fclose(icon);

		} else if (!strcmp(path, "/dwarves")) {

			os_mutex_lock(&world_instance->lock);
			update_to_now(world_instance);
			int status = render_dwarves(world_instance->world, body);
			os_mutex_unlock(&world_instance->lock);

			send_response(client_socket, "text/html", status, body, strlen(body));

		} else if (!strcmp(path, "/feed")) {

			os_mutex_lock(&world_instance->lock);
			update_to_now(world_instance);
			int status = render_feed(world_instance->world, body);
			os_mutex_unlock(&world_instance->lock);

			send_text_response(client_socket, "text/html", status, body);

			// TODO: Seriously need a real routing scheme
		} else if (sscanf(path, "/entities/%d", &id) == 1 && strstr(path, "avatar.svg")) {

			os_mutex_lock(&world_instance->lock);
			update_to_now(world_instance);
			int status = render_entity_avatar(world_instance->world, id, body);
			os_mutex_unlock(&world_instance->lock);

			send_text_response(client_socket, "image/svg+xml", status, body);

		} else if (sscanf(path, "/entities/%d", &id) == 1) {

			os_mutex_lock(&world_instance->lock);
			update_to_now(world_instance);
			int status = render_entity(world_instance->world, id, body);
			os_mutex_unlock(&world_instance->lock);

			send_text_response(client_socket, "text/html", status, body);

		} else if (!strcmp(path, "/locations")) {

			os_mutex_lock(&world_instance->lock);
			update_to_now(world_instance);
			int status = render_locations(world_instance->world, body);
			os_mutex_unlock(&world_instance->lock);

			send_text_response(client_socket, "text/html", status, body);

		} else if (sscanf(path, "/locations/%d", &id) == 1) {

			os_mutex_lock(&world_instance->lock);
			update_to_now(world_instance);
			int status = render_location(world_instance->world, id, body);
			os_mutex_unlock(&world_instance->lock);

			send_text_response(client_socket, "text/html", status, body);

		} else if (!strcmp(path, "/stats")) {

			os_mutex_lock(&global_stats.lock);
			int status = render_stats(&global_stats, body);
			os_mutex_unlock(&global_stats.lock);

			send_text_response(client_socket, "text/html", status, body);

		} else if (!strcmp(path, "/heap")) {

			int status = render_heap(body);

			send_text_response(client_socket, "text/html", status, body);

		} else if (sscanf(path, "/allocations/%llu", &serial) == 1) {

			int status = render_allocation(body, serial);

			send_text_response(client_socket, "text/html", status, body);

		} else if (!strcmp(path, "/allocations")) {

			int status = render_allocations(body);

			send_text_response(client_socket, "text/html", status, body);

		} else if (sscanf(path, "/test/%s", test_name) == 1) {

			char *in_buffer = M_ALLOC(char, TEST_BUFFER_SIZE);
			char *out_buffer = M_ALLOC(char, TEST_BUFFER_SIZE);

			buffer_limit(&buffer, TEST_BUFFER_SIZE);

			// TODO: This is bad.
			buffer_read_amount(&buffer, in_buffer, content_length);

			size_t leak_amount;
			size_t out_length = test_call(test_name, out_buffer,
				in_buffer, content_length, &leak_amount);

			char leak_header[128];
			size_t len = sprintf(leak_header, "X-Memory-Leak: %d", (int)leak_amount);

			String extra_headers[] = {
				to_string(leak_header, len),
			};

			send_response(client_socket, "application/octet-stream", 200,
				out_buffer, out_length,
				extra_headers, Count(extra_headers));

			M_FREE(in_buffer);
			M_FREE(out_buffer);

		} else if (!strcmp(path, "/")) {
			const char *body = "<html><body><h1>Hello world!</h1></body></html>";
			send_text_response(client_socket, "text/html", 200, body);
		} else {
			const char *body = "<html><body><h1>Not found.</h1></body></html>";
			send_text_response(client_socket, "text/html", 404, body);
		}

		float ms = os_timer_delta_ms(begin_respond, os_get_timer());
		printf("%d: Request %s %s (took %.2f ms)\n", data->thread_id, method, path, ms);
	}

	os_socket_stop_recv(client_socket);
	os_socket_close(client_socket);

	buffer_free(&buffer);
	M_FREE(body);
	M_FREE(thread_data);

	os_atomic_decrement(&active_thread_count);

	OS_THREAD_RETURN;
}

int main(int argc, char **argv)
{

	os_startup(argc, argv);

#if BUILD_DEBUG
	debug_alloc_init();
#endif

	static char err_buffer[128];

	signal(SIGINT, handle_kill);
	signal(SIGTERM, handle_kill);

	global_stats.snapshot_count = 100;
	global_stats.active_thread_counts = M_ALLOC_ZERO(long, global_stats.snapshot_count);
	os_mutex_init(&global_stats.lock);

	struct addrinfo *addr = NULL;
	struct addrinfo hints = { 0 };
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, DORF_PORT, &hints, &addr);

	server_socket = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	if (bind(server_socket, addr->ai_addr, (int)addr->ai_addrlen)) {
		os_socket_format_last_error(err_buffer, sizeof(err_buffer));
		printf("Failed to bind socket: %s\n", err_buffer);
	}
	if (listen(server_socket, SOMAXCONN)) {
		os_socket_format_last_error(err_buffer, sizeof(err_buffer));
		printf("Failed to bind socket: %s\n", err_buffer);
	}

	freeaddrinfo(addr);

	const char *names[] = {
		"Urist", "Gimli", "Thir", "Tharun", "Dofor", "Ufir",
		"Bohir",
	};

	puts("Dorfbook serving at port " DORF_PORT);
	puts("Enter ^C to stop");

	char name_buf[512], *name_ptr = name_buf;

	static World world = { 0 };
	world.random_series = series_from_seed32(0xD02F);

	world.locations[1].id = 1;
	world.locations[1].name = "Initial Cave";
	world.locations[2].id = 2;
	world.locations[2].name = "The Great Outdoors";
	world.locations[3].id = 3;
	world.locations[3].name = "Some Pub";
	world.locations[3].has_food = true;
	world.locations[4].id = 4;
	world.locations[4].name = "Bedroom";
	world.locations[4].has_bed = true;

	for (U32 id = 1; id < 10; id++) {
		char *name = name_ptr;

		U32 first_name_index = next32(&world.random_series) % Count(names);
		U32 last_name_index = next32(&world.random_series) % Count(names);

		name_ptr += 1 + sprintf(name_ptr, "%s %sson",
			names[first_name_index], names[last_name_index]);

		Dwarf *dwarf = &world.dwarves[id];
		dwarf->id = id;
		dwarf->location = 1;
		dwarf->name = name;
		dwarf->hunger = next32(&world.random_series) % 50;
		dwarf->sleep = next32(&world.random_series) % 50;
		dwarf->alive = true;
		dwarf->seed = next32(&world.random_series);
	}

	Assets assets = { 0 };
	static char face_xml_data[1024*1024];

	FILE *face_xml_file = fopen("data/faces.svg", "r");
	size_t num_bytes = fread(face_xml_data, 1, sizeof(face_xml_data), face_xml_file);

	if (!parse_xml(&assets.faces.xml, face_xml_data, num_bytes)) {
		puts("Failed to parse face SVG");
	}

	fclose(face_xml_file);

	initialize_id_list(&assets.faces);

	world.assets = &assets;

	World_Instance world_instance = { 0 };
	world_instance.last_updated = time(NULL);
	world_instance.world = &world;
	os_mutex_init(&world_instance.lock);

	os_thread_do(thread_background_world_update, &world_instance);
	os_thread_do(thread_background_stat_update, &global_stats);

	int thread_id = 0;

	for (;;) {
		os_socket client_socket = accept(server_socket, NULL, NULL);
		if (!os_valid_socket(client_socket))
			continue;

		// Don't block on any function for longer than 15 seconds.
		int timeout = 15;
		if (!os_socket_set_timeout(client_socket, timeout, timeout)) {
			os_socket_format_last_error(err_buffer, sizeof(err_buffer));
			printf("Failed to set socket timeout: %s\n", err_buffer);
			continue;
		}

		Response_Thread_Data *thread_data = M_ALLOC(Response_Thread_Data, 1);
		thread_data->client_socket = client_socket;
		thread_data->world_instance = &world_instance;
		thread_data->body_storage = M_ALLOC(char, 1024*1024);
		thread_data->thread_id = ++thread_id;

#if 1
		os_thread_do(thread_do_response, thread_data);
#else
		thread_do_response(thread_data);
#endif
	}
}

