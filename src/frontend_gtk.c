#ifdef GBMU_USE_GTK

#include <gtk/gtk.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <sys/time.h>
#include <math.h>
#include "emu.h"

#define SCREEN_SCALE 2
#define DEBUG_SCREEN_SCALE 2

enum {PLAY, PAUSE} state;
GtkWidget *window;
GtkWidget *image;
GtkWidget *image_debug;
GtkWidget *txtview;
GtkTextBuffer *txtbuf;
bool *keyboard_state;
GtkWidget *btn_load;
GtkWidget *btn_pause;
GtkWidget *btn_reset;
GtkWidget *btn_run_frame;
GtkWidget *btn_run_instr;
GtkWidget *btn_force_dmg_gbc;
GtkWidget *frame_speed_control, *box_speed_control, *radio1, *radio2, *radio3, *radio4;
bool running;
int64_t time_called_pulseaudio_write;
pthread_mutex_t my_mutex;

void my_quit() {
	running = false;
}

bool key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event->keyval < 0x10000)
		keyboard_state[event->keyval] = TRUE;
	if (event->keyval == GDK_KEY_Escape)
		my_quit();
	else if (event->keyval == GDK_KEY_Pause)
		gtk_button_clicked((GtkButton*)btn_pause);
	else if (event->keyval == GDK_KEY_KP_Multiply)
		gtk_button_clicked((GtkButton*)btn_reset);
	else if (event->keyval == GDK_KEY_F3) {
		printf("pressed f3\n");
	}
	return TRUE;
}

bool key_release_event(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event->keyval < 0x10000)
		keyboard_state[event->keyval] = FALSE;
	return TRUE;
}

void refresh_screen() {
	// Screen
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_data(
		(const guchar*)screen_pixels, GDK_COLORSPACE_RGB,
		true, 8, 160, 144, 160*4, NULL, NULL);
	GdkPixbuf *pixbuf_scaled = gdk_pixbuf_scale_simple(
		pixbuf, 160*SCREEN_SCALE, 144*SCREEN_SCALE, GDK_INTERP_NEAREST);
	gtk_image_set_from_pixbuf((GtkImage*)image, pixbuf_scaled);
	g_object_unref(pixbuf);
	g_object_unref(pixbuf_scaled);
	// Tiles debug
	gbmu_update_debug_tiles_screen();
	pixbuf = gdk_pixbuf_new_from_data(
		(const guchar*)screen_debug_tiles_pixels, GDK_COLORSPACE_RGB,
		true, 8, SCREEN_DEBUG_TILES_W, SCREEN_DEBUG_TILES_H, SCREEN_DEBUG_TILES_W*4, NULL, NULL);
	pixbuf_scaled = gdk_pixbuf_scale_simple(
		pixbuf, SCREEN_DEBUG_TILES_W * DEBUG_SCREEN_SCALE, SCREEN_DEBUG_TILES_H * DEBUG_SCREEN_SCALE, GDK_INTERP_NEAREST);
	gtk_image_set_from_pixbuf((GtkImage*)image_debug, pixbuf_scaled);
	g_object_unref(pixbuf);
	g_object_unref(pixbuf_scaled);
	// Debug text
	gtk_text_buffer_set_text(txtbuf, gbmu_debug_info(), -1);
}

void update_input() {
	gbmu_keys[GBMU_A] = keyboard_state[GDK_KEY_k];
	gbmu_keys[GBMU_B] = keyboard_state[GDK_KEY_j];
	gbmu_keys[GBMU_START] = keyboard_state[GDK_KEY_Return];
	gbmu_keys[GBMU_SELECT] = keyboard_state[GDK_KEY_Shift_L] || keyboard_state[GDK_KEY_Shift_R];
	gbmu_keys[GBMU_UP] = keyboard_state[GDK_KEY_z] || keyboard_state[GDK_KEY_w];
	gbmu_keys[GBMU_LEFT] = keyboard_state[GDK_KEY_q] || keyboard_state[GDK_KEY_a];
	gbmu_keys[GBMU_DOWN] = keyboard_state[GDK_KEY_s];
	gbmu_keys[GBMU_RIGHT] = keyboard_state[GDK_KEY_d];
}

void update_buttons() {
	switch (state) {
		case PLAY:
			gtk_button_set_label((GtkButton*)btn_pause, "Pause");
			gtk_widget_set_sensitive(btn_run_frame, false);
			gtk_widget_set_sensitive(btn_run_instr, false);
			break;
		case PAUSE:
			gtk_button_set_label((GtkButton*)btn_pause, "Play");
			gtk_widget_set_sensitive(btn_run_frame, true);
			gtk_widget_set_sensitive(btn_run_instr, true);
			break;
	}
	if (hardwareMode == MODE_DMG)
		gtk_button_set_label((GtkButton*)btn_force_dmg_gbc, "Force GBC");
	else if (hardwareMode == MODE_GBC)
		gtk_button_set_label((GtkButton*)btn_force_dmg_gbc, "Force DMG");
}

void load_rom(char *filename) {
	gbmu_load_rom(filename);
	state = PLAY;
	update_buttons();
}

bool btn_load_clicked(GtkWidget *widget, GdkEventKey *event, gpointer data) {
	GtkWidget *dialog = gtk_file_chooser_dialog_new ("Open File",
			(GtkWindow*)window,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			"Cancel", GTK_RESPONSE_CANCEL,
			"Open", GTK_RESPONSE_ACCEPT,
			NULL);
	int res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res == GTK_RESPONSE_ACCEPT) {
		char *filename;
		GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
		filename = gtk_file_chooser_get_filename (chooser);
		load_rom(filename);
		g_free (filename);
	}
	gtk_widget_destroy (dialog);
}

bool btn_pause_clicked(GtkWidget *widget, GdkEventKey *event, gpointer data) {
	if (state == PLAY)
		state = PAUSE;
	else if (state == PAUSE)
		state = PLAY;
	update_buttons();
}

bool btn_reset_clicked(GtkWidget *widget, GdkEventKey *event, gpointer data) {
	gbmu_reset();
	refresh_screen();
}

bool btn_run_frame_clicked(GtkWidget *widget, GdkEventKey *event, gpointer data) {
	update_input();
	gbmu_run_one_frame();
	refresh_screen();
}

bool btn_run_instr_clicked(GtkWidget *widget, GdkEventKey *event, gpointer data) {
	update_input();
	gbmu_run_one_instr();
	refresh_screen();
}


void drag_data_received(GtkWidget *widget, GdkDragContext *context, int x, int y,
	GtkSelectionData *data, guint info, guint time, gpointer user_data) {
	char **uris = gtk_selection_data_get_uris(data);
	char *filename = NULL;
	if (uris) {
		filename = g_filename_from_uri(uris[0], NULL, NULL);
	}
	printf("loading rom: %s\n", filename);
	load_rom(filename);
	g_strfreev(uris);
	g_free(filename);
	gtk_drag_finish(context, true, false, time);
}

void btn_force_dmg_gbc_clicked() {
	if (hardwareMode == MODE_DMG)
		hardwareMode = MODE_GBC;
	else if (hardwareMode == MODE_GBC)
		hardwareMode = MODE_DMG;
	gbmu_reset();
	update_buttons();
	refresh_screen();
}

int64_t currentTimeMillis() {
	struct timeval time;
	gettimeofday(&time, NULL);
	int64_t sec = (int64_t)(time.tv_sec) * 1000;
	int64_t milli = (time.tv_usec / 1000);
	return sec + milli;
}

void frame_is_ready() {

	while (gtk_events_pending())
		gtk_main_iteration_do(false);

	if (!running)
		return;

	refresh_screen();

	update_input();

	if (gtk_toggle_button_get_active((GtkToggleButton*)radio1))
		emulation_speed = 0.5;
	else if (gtk_toggle_button_get_active((GtkToggleButton*)radio2))
		emulation_speed = 1.0;
	else if (gtk_toggle_button_get_active((GtkToggleButton*)radio3))
		emulation_speed = 2.0;
}

void set_time_called_pulseaudio_write(int64_t time) {
	pthread_mutex_lock(&my_mutex);
	time_called_pulseaudio_write = time;
	pthread_mutex_unlock(&my_mutex);
}

int64_t get_time_called_pulseaudio_write() {
	pthread_mutex_lock(&my_mutex);
	int64_t ret = time_called_pulseaudio_write;
	pthread_mutex_unlock(&my_mutex);
	return ret;
}

void *check_that_pulseaudio_write_isnt_stuck(void *arg) {
	(void)arg;
	while (true) {
		int64_t t = get_time_called_pulseaudio_write();
		int64_t timeout = 3000;
		if (t && ((currentTimeMillis() - t) > timeout))
		{
			printf(
				"PulseAudio seems to be stuck.\n"
				"This is a known bug in VirtualBox. https://www.virtualbox.org/ticket/18594.\n"
				"Workarounds:\n"
				"- reboot guest system or\n"
				"- sleep and wake up guest system (faster than reboot)\n"
			);
			exit(EXIT_FAILURE);
		}
		sleep(1);
	}
}


int main(int ac, char **av) {
	if (!(keyboard_state = calloc(0x10000, sizeof(bool)))) {
		perror("calloc");
		exit(EXIT_FAILURE);
	}

	gbmu_init();

	gtk_init(&ac, &av);
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_position((GtkWindow*)window, GTK_WIN_POS_CENTER);
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(my_quit), NULL);
	g_signal_connect(window, "key_press_event", G_CALLBACK(key_press_event), NULL);
	g_signal_connect(window, "key_release_event", G_CALLBACK(key_release_event), NULL);
	g_signal_connect(window, "drag_data_received", G_CALLBACK(drag_data_received), NULL);
	gtk_drag_dest_set(window, GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_MOVE);
	gtk_drag_dest_add_uri_targets(window);

	GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_add(GTK_CONTAINER(window), hbox);

	GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);

	btn_load = gtk_button_new_with_label("Load");
	g_signal_connect(btn_load, "clicked", G_CALLBACK(btn_load_clicked), NULL);
	gtk_box_pack_start((GtkBox*)vbox, btn_load, 0, 0, 0);

	btn_pause = gtk_button_new_with_label("Pause");
	g_signal_connect(btn_pause, "clicked", G_CALLBACK(btn_pause_clicked), NULL);
	gtk_box_pack_start((GtkBox*)vbox, btn_pause, 0, 0, 0);

	btn_reset = gtk_button_new_with_label("Reset");
	g_signal_connect(btn_reset, "clicked", G_CALLBACK(btn_reset_clicked), NULL);
	gtk_box_pack_start((GtkBox*)vbox, btn_reset, 0, 0, 0);

	btn_run_frame = gtk_button_new_with_label("Run frame");
	g_signal_connect(btn_run_frame, "clicked", G_CALLBACK(btn_run_frame_clicked), NULL);
	gtk_box_pack_start((GtkBox*)vbox, btn_run_frame, 0, 0, 0);

	btn_run_instr = gtk_button_new_with_label("Run instr");
	g_signal_connect(btn_run_instr, "clicked", G_CALLBACK(btn_run_instr_clicked), NULL);
	gtk_box_pack_start((GtkBox*)vbox, btn_run_instr, 0, 0, 0);

	btn_force_dmg_gbc = gtk_button_new();
	g_signal_connect(btn_force_dmg_gbc, "clicked", G_CALLBACK(btn_force_dmg_gbc_clicked), NULL);
	gtk_box_pack_start((GtkBox*)vbox, btn_force_dmg_gbc, 0, 0, 0);

	frame_speed_control = gtk_frame_new ("Speed");
	gtk_widget_set_margin_top(frame_speed_control, 10);
	gtk_box_pack_start((GtkBox*)vbox, frame_speed_control, 0, 0, 0);

	box_speed_control = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (frame_speed_control), box_speed_control);

	radio1 = gtk_radio_button_new_with_label(NULL, "x0.5");
	gtk_box_pack_start (GTK_BOX (box_speed_control), radio1, 0, 0, 0);
	radio2 = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (radio1), "x1.0");
	gtk_box_pack_start (GTK_BOX (box_speed_control), radio2, 0, 0, 0);
	radio3 = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (radio1), "x2.0");
	gtk_box_pack_start (GTK_BOX (box_speed_control), radio3, 0, 0, 0);
	gtk_toggle_button_set_active((GtkToggleButton*)radio2, true);

	gtk_container_add(GTK_CONTAINER(hbox), gtk_separator_new(0));

	image = gtk_image_new();
	gtk_widget_set_size_request(image, 160*SCREEN_SCALE, 144*SCREEN_SCALE);
	gtk_container_add(GTK_CONTAINER(hbox), image);
	gtk_container_add(GTK_CONTAINER(hbox), gtk_separator_new(0));

	image_debug = gtk_image_new();
	gtk_widget_set_size_request(image_debug, SCREEN_DEBUG_TILES_W * DEBUG_SCREEN_SCALE,
		SCREEN_DEBUG_TILES_H * DEBUG_SCREEN_SCALE);
	gtk_container_add(GTK_CONTAINER(hbox), image_debug);
	gtk_container_add(GTK_CONTAINER(hbox), gtk_separator_new(0));

	txtview = gtk_text_view_new();
	gtk_text_view_set_top_margin((GtkTextView*)txtview, 5);
	gtk_text_view_set_bottom_margin((GtkTextView*)txtview, 5);
	gtk_text_view_set_left_margin((GtkTextView*)txtview, 5);
	gtk_text_view_set_right_margin((GtkTextView*)txtview, 5);
	gtk_text_view_set_cursor_visible((GtkTextView*)txtview, false);
	gtk_text_view_set_monospace((GtkTextView*)txtview, true);
	txtbuf = gtk_text_view_get_buffer((GtkTextView*)txtview);
	gtk_container_add(GTK_CONTAINER(hbox), txtview);

	load_rom(av[1]);

	update_buttons();
	gtk_widget_show_all(window);

	#define AUDIO_BUF_COUNT 60
	#define AUDIO_BUF_SIZE (AUDIO_BUF_COUNT * sizeof(float))

	pa_sample_spec ss;
	ss.format = PA_SAMPLE_FLOAT32;
	ss.channels = 1;
	ss.rate = 44100;

	pa_buffer_attr attr;
	attr.maxlength = AUDIO_BUF_SIZE;
	attr.tlength = AUDIO_BUF_SIZE;
	attr.prebuf = AUDIO_BUF_SIZE;
	attr.minreq = 0;
	attr.fragsize = (uint32_t) -1;

	pa_simple *s;
	int error;
	if (!(s = pa_simple_new(NULL, av[0], PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, &attr, &error))) {
		fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
		exit(EXIT_FAILURE);
	}
	num_audio_channels = ss.channels;
	uint8_t *audio_buf;
	if (!(audio_buf = malloc(AUDIO_BUF_SIZE))) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	if (pthread_mutex_init(&my_mutex, NULL)) {
		puts("mutex init has failed");
		exit(EXIT_FAILURE);
	}
	pthread_t thread;
	if ((pthread_create(&thread, NULL, check_that_pulseaudio_write_isnt_stuck, NULL))) {
		puts("pthread_create error");
		exit(EXIT_FAILURE);
	}

	running = true;
	while (running) {

		if (state == PLAY) {
			gbmu_fill_audio_buffer(audio_buf, AUDIO_BUF_SIZE, &frame_is_ready);
			set_time_called_pulseaudio_write( currentTimeMillis() );
			if (pa_simple_write(s, audio_buf, AUDIO_BUF_SIZE, &error) < 0) {
				fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
				exit(EXIT_FAILURE);
			}
			set_time_called_pulseaudio_write( 0 );
		}
		else {
			pa_simple_flush(s, NULL);
			frame_is_ready();
			usleep(16000);
		}
	}

	free(keyboard_state);
	free(audio_buf);
	pa_simple_free(s);
	gbmu_quit();
}

#endif
