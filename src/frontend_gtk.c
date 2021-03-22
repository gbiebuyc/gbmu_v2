#include <gtk/gtk.h>
#include <stdio.h>
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
bool running;

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

	GtkWidget *frame_speed_control, *box_speed_control, *radio1, *radio2, *radio3, *radio4;

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
	radio4 = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (radio1), "Unlimited");
	gtk_box_pack_start (GTK_BOX (box_speed_control), radio4, 0, 0, 0);
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

	running = true;
	while (running) {

		struct timeval before_tv, after_tv;
		gettimeofday(&before_tv, NULL);

		while (gtk_events_pending())
			gtk_main_iteration_do(false);

		if (!running)
			break;

		if (state == PLAY) {
			update_input();
			gbmu_run_one_frame();
			refresh_screen();
		}

		double speed;
		if (gtk_toggle_button_get_active((GtkToggleButton*)radio1))
			speed = 0.5;
		else if (gtk_toggle_button_get_active((GtkToggleButton*)radio2))
			speed = 1.0;
		else if (gtk_toggle_button_get_active((GtkToggleButton*)radio3))
			speed = 2.0;
		else if (gtk_toggle_button_get_active((GtkToggleButton*)radio4))
			speed = INFINITY;

		int frame_duration = 1000000.0 / (60.0 * speed);
		// printf("frame duration: %d\n", frame_duration);

		gettimeofday(&after_tv, NULL);
		int64_t before_msec = ((int64_t)before_tv.tv_sec)*1000000 + before_tv.tv_usec;
		int64_t after_msec = ((int64_t)after_tv.tv_sec)*1000000 + after_tv.tv_usec;
		int64_t elapsed_time = after_msec - before_msec;
		if (elapsed_time < frame_duration)
			usleep(frame_duration - elapsed_time);
	}

	free(keyboard_state);
	gbmu_quit();
}
