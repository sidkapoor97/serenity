/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021, Gunnar Beutner <gbeutner@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibGUI/Action.h>
#include <LibGUI/Application.h>
#include <LibGUI/Icon.h>
#include <LibGUI/Menu.h>
#include <LibGUI/Menubar.h>
#include <LibGUI/Painter.h>
#include <LibGUI/Widget.h>
#include <LibGUI/Window.h>
#include <LibGfx/Bitmap.h>
#include <unistd.h>

class MandelbrotSet {
public:
    MandelbrotSet()
    {
        set_view();
    }

    void reset()
    {
        set_view();
        calculate();
    }

    void resize(Gfx::IntSize const& size)
    {
        m_bitmap = Gfx::Bitmap::create(Gfx::BitmapFormat::BGRx8888, size);
        calculate();
    }

    void zoom(Gfx::IntRect const& rect)
    {
        set_view(
            rect.left() * (m_x_end - m_x_start) / m_bitmap->width() + m_x_start,
            rect.right() * (m_x_end - m_x_start) / m_bitmap->width() + m_x_start,
            rect.top() * (m_y_end - m_y_start) / m_bitmap->height() + m_y_start,
            rect.bottom() * (m_y_end - m_y_start) / m_bitmap->height() + m_y_start);
        calculate();
    }

    i32 mandelbrot(double px, double py, i32 max_iterations)
    {
        // Based on https://en.wikipedia.org/wiki/Plotting_algorithms_for_the_Mandelbrot_set
        const double x0 = px * (m_x_end - m_x_start) / m_bitmap->width() + m_x_start;
        const double y0 = py * (m_y_end - m_y_start) / m_bitmap->height() + m_y_start;
        double x = 0;
        double y = 0;
        i32 iteration = 0;
        double x2 = 0;
        double y2 = 0;

        while (x2 + y2 <= 4 && iteration < max_iterations) {
            y = 2 * x * y + y0;
            x = x2 - y2 + x0;
            x2 = x * x;
            y2 = y * y;
            iteration++;
        }

        return iteration;
    }

    void calculate_pixel(int px, int py, int max_iterations)
    {
        auto iterations = mandelbrot(px, py, max_iterations);
        double hue = (double)iterations * 360.0 / (double)max_iterations;
        if (hue == 360.0)
            hue = 0.0;
        double saturation = 1.0;
        double value = iterations < max_iterations ? 1.0 : 0;
        m_bitmap->set_pixel(px, py, Color::from_hsv(hue, saturation, value));
    }

    void calculate(int max_iterations = 100)
    {
        if (!m_bitmap)
            return;

        for (int py = 0; py < m_bitmap->height(); py++)
            for (int px = 0; px < m_bitmap->width(); px++)
                calculate_pixel(px, py, max_iterations);
    }

    Gfx::Bitmap const& bitmap() const
    {
        return *m_bitmap;
    }

private:
    double m_x_start { 0 };
    double m_x_end { 0 };
    double m_y_start { 0 };
    double m_y_end { 0 };
    RefPtr<Gfx::Bitmap> m_bitmap;

    void set_view(double x_start = -2.5, double x_end = 1.0, double y_start = -1.0, double y_end = 1.0)
    {
        m_x_start = x_start;
        m_x_end = x_end;
        m_y_start = y_start;
        m_y_end = y_end;
    }
};

class Mandelbrot : public GUI::Widget {
    C_OBJECT(Mandelbrot)
private:
    virtual void paint_event(GUI::PaintEvent&) override;
    virtual void mousedown_event(GUI::MouseEvent& event) override;
    virtual void mousemove_event(GUI::MouseEvent& event) override;
    virtual void mouseup_event(GUI::MouseEvent& event) override;
    virtual void resize_event(GUI::ResizeEvent& event) override;

    bool m_dragging { false };
    Gfx::IntPoint m_selection_start;
    Gfx::IntPoint m_selection_end;

    MandelbrotSet m_set;
};

void Mandelbrot::paint_event(GUI::PaintEvent& event)
{
    GUI::Painter painter(*this);
    painter.add_clip_rect(event.rect());
    painter.draw_scaled_bitmap(rect(), m_set.bitmap(), m_set.bitmap().rect());

    if (m_dragging)
        painter.draw_rect(Gfx::IntRect::from_two_points(m_selection_start, m_selection_end), Color::Blue);
}

void Mandelbrot::mousedown_event(GUI::MouseEvent& event)
{
    if (event.button() == GUI::MouseButton::Left) {
        if (!m_dragging) {
            m_selection_start = event.position();
            m_selection_end = event.position();
            m_dragging = true;
            update();
        }
    }

    return GUI::Widget::mousedown_event(event);
}

void Mandelbrot::mousemove_event(GUI::MouseEvent& event)
{
    if (m_dragging) {
        m_selection_end = event.position();
        update();
    }

    return GUI::Widget::mousemove_event(event);
}

void Mandelbrot::mouseup_event(GUI::MouseEvent& event)
{
    if (event.button() == GUI::MouseButton::Left) {
        auto selection = Gfx::IntRect::from_two_points(m_selection_start, m_selection_end);
        if (selection.width() > 0 && selection.height() > 0)
            m_set.zoom(selection);
        m_dragging = false;
        update();
    } else if (event.button() == GUI::MouseButton::Right) {
        m_set.reset();
        update();
    }

    return GUI::Widget::mouseup_event(event);
}

void Mandelbrot::resize_event(GUI::ResizeEvent& event)
{
    m_set.resize(event.size());
}

int main(int argc, char** argv)
{
    auto app = GUI::Application::construct(argc, argv);

    if (pledge("stdio recvfd sendfd rpath", nullptr) < 0) {
        perror("pledge");
        return 1;
    }

    if (unveil("/res", "r") < 0) {
        perror("unveil");
        return 1;
    }

    if (unveil(nullptr, nullptr) < 0) {
        perror("unveil");
        return 1;
    }

    auto window = GUI::Window::construct();
    window->set_double_buffering_enabled(false);
    window->set_title("Mandelbrot");
    window->set_minimum_size(320, 240);
    window->resize(window->minimum_size() * 2);

    auto menubar = GUI::Menubar::construct();
    auto& file_menu = menubar->add_menu("&File");
    file_menu.add_action(GUI::CommonActions::make_quit_action([&](auto&) { app->quit(); }));
    window->set_menubar(move(menubar));
    window->set_main_widget<Mandelbrot>();
    window->show();

    auto app_icon = GUI::Icon::default_icon("app-mandelbrot");
    window->set_icon(app_icon.bitmap_for_size(16));

    return app->exec();
}
