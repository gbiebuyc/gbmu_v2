#include <gtk/gtk.h>
#include <stdio.h>
#include "../src_emu/emu.h"

#define SCREEN_SCALE 2

GtkWidget *window;
GtkWidget *image;
GtkWidget *txtview;
GtkTextBuffer *txtbuf;
bool isPlaying;
bool *keyboard_state;

bool key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event->keyval == GDK_KEY_Escape)
		gtk_main_quit();
	if (event->keyval < 0x10000)
		keyboard_state[event->keyval] = TRUE;
	return TRUE;
}

bool key_release_event(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event->keyval < 0x10000)
		keyboard_state[event->keyval] = FALSE;
	return TRUE;
}

bool timeout_cb(void *data) {
	if (isPlaying) {
		gbmu_keys[GBMU_A] = keyboard_state[GDK_KEY_k];
		gbmu_keys[GBMU_B] = keyboard_state[GDK_KEY_j];
		gbmu_keys[GBMU_START] = keyboard_state[GDK_KEY_Return];
		gbmu_keys[GBMU_SELECT] = keyboard_state[GDK_KEY_BackSpace];
		gbmu_keys[GBMU_UP] = keyboard_state[GDK_KEY_z] || keyboard_state[GDK_KEY_w];
		gbmu_keys[GBMU_LEFT] = keyboard_state[GDK_KEY_q] || keyboard_state[GDK_KEY_a];
		gbmu_keys[GBMU_DOWN] = keyboard_state[GDK_KEY_s];
		gbmu_keys[GBMU_RIGHT] = keyboard_state[GDK_KEY_d];
		gbmu_run_one_frame();
		GdkPixbuf *pixbuf = gdk_pixbuf_new_from_data(
			(const guchar*)screen_pixels, GDK_COLORSPACE_RGB,
			true, 8, 160, 144, 160*4, NULL, NULL);
		GdkPixbuf *pixbuf_scaled = gdk_pixbuf_scale_simple(
			pixbuf, 160*SCREEN_SCALE, 144*SCREEN_SCALE, GDK_INTERP_NEAREST);
		gtk_image_set_from_pixbuf((GtkImage*)image, pixbuf_scaled);
		g_object_unref(pixbuf);
		g_object_unref(pixbuf_scaled);
		char flag_str[4];
		flag_str[0] = (regs.F & 0x80) ? 'Z' : '-';
		flag_str[1] = (regs.F & 0x40) ? 'N' : '-';
		flag_str[2] = (regs.F & 0x20) ? 'H' : '-';
		flag_str[3] = (regs.F & 0x10) ? 'C' : '-';
		char buf[1000];
		char *b = buf;
		b += sprintf(b, "af= %04X  ", regs.AF);
		b += sprintf(b, "lcdc=%02X\n", mem[0xff40]);
		b += sprintf(b, "bc= %04X  ", regs.BC);
		b += sprintf(b, "stat=%02X\n", mem[0xff41]);
		b += sprintf(b, "de= %04X  ", regs.DE);
		b += sprintf(b, "ly=  %02X\n", mem[0xff44]);
		b += sprintf(b, "hl= %04X  ", regs.HL);
		b += sprintf(b, "cnt= %02X\n", mem[0xff05]);
		b += sprintf(b, "sp= %04X  ", SP);
		b += sprintf(b, "ie=  %02X\n", mem[0xffff]);
		b += sprintf(b, "pc= %04X  ", PC);
		b += sprintf(b, "if=  %02X\n", mem[0xff0f]);
		b += sprintf(b, "ime=%d  ", IME);
		b += sprintf(b, "flags=%.4s\n", flag_str);
		gtk_text_buffer_set_text(txtbuf, buf, -1);
	}
	return true;
}

void load_rom(char *filename) {
	if (gbmu_load_rom(filename)) {
		isPlaying = true;
	} else {
		GtkWidget *dialog = gtk_message_dialog_new (
			(GtkWindow*)window,
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_WARNING,
			GTK_BUTTONS_CLOSE,
			"Error loading rom.");
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}
}

bool load_btn_clicked(GtkWidget *widget, GdkEventKey *event, gpointer data) {
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

bool pause_btn_clicked(GtkWidget *widget, GdkEventKey *event, gpointer data) {
	if (isPlaying) {
		isPlaying = false;
		gtk_button_set_label((GtkButton*)widget, "Play");
	} else {
		isPlaying = true;
		gtk_button_set_label((GtkButton*)widget, "Pause");
	}
}

int main(int ac, char **av) {
	keyboard_state = calloc(0x10000, sizeof(bool));

	gtk_init(&ac, &av);
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(window, "key_press_event", G_CALLBACK(key_press_event), NULL);
	g_signal_connect(window, "key_release_event", G_CALLBACK(key_release_event), NULL);

	GtkWidget *container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(window), container);

	image = gtk_image_new();
	gtk_widget_set_size_request(image, 160*SCREEN_SCALE, 144*SCREEN_SCALE);
	gtk_container_add(GTK_CONTAINER(container), image);

	GtkWidget *load_button = gtk_button_new_with_label("Load");
	g_signal_connect(load_button, "clicked", G_CALLBACK(load_btn_clicked), NULL);
	gtk_container_add(GTK_CONTAINER(container), load_button);

	GtkWidget *pause_button = gtk_button_new_with_label("Pause");
	g_signal_connect(pause_button, "clicked", G_CALLBACK(pause_btn_clicked), NULL);
	gtk_container_add(GTK_CONTAINER(container), pause_button);

	txtview = gtk_text_view_new();
	gtk_text_view_set_cursor_visible((GtkTextView*)txtview, FALSE);
	gtk_text_view_set_monospace((GtkTextView*)txtview, TRUE);
	txtbuf = gtk_text_view_get_buffer((GtkTextView*)txtview);
	gtk_container_add(GTK_CONTAINER(container), txtview);

	if (av[1]) {
		load_rom(av[1]);
	}

	g_timeout_add_full(G_PRIORITY_HIGH, 16, (GSourceFunc)timeout_cb, NULL, NULL);
	gtk_widget_show_all(window);
	gtk_main();
}
