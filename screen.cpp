#include "screen.hpp"
#include "base_text_buffer.hpp"
#include "painter.hpp"
#include "key_event.hpp"
#include "resize_event.hpp"
#include "text_input_event.hpp"
#include "to_utf16.hpp"
#include <SDL2/SDL_ttf.h>
#include <iostream>

Screen::Char::Char(wchar_t ach, Color afg, Color abg):
    ch(ach),
    fg(afg),
    bg(abg)
{}

Screen::Screen(Widget *parent):
    Widget(parent),
    cursor_{0, 0},
    startSelection_{-1, -1},
    endSelection_{-1, -1},
    hScroll_{0},
    vScroll_{0},
    textBuffer_{nullptr}
{
    Painter p(this);
    glyphWidth_ = p.glyphWidth();
    glyphHeight_ = p.glyphHeight();
    ResizeEvent e{width(), height()};
    resizeEvent(e);
}

void Screen::paintEvent(PaintEvent &)
{
    Painter p(this);
    for (size_t y = 0; y < ch_.size(); ++y)
        for (size_t x = 0; x < ch_[y].size(); ++x)
        {
            const auto &c = ch_[y][x];
            p.renderGlyph(c.ch, x * glyphWidth_, y * glyphHeight_, c.fg, c.bg);
        }
    p.setColor(Gray);
    auto xx = cursor_.x - hScroll_;
    auto yy = cursor_.y - vScroll_;
    p.drawLine(xx * glyphWidth_, yy * glyphHeight_,
               xx * glyphWidth_, (yy + 1) * glyphHeight_);
}

bool Screen::keyPressEvent(KeyEvent &e)
{
    if (!textBuffer_)
        return false;
    switch (e.modifiers()) 
    {
    case KeyEvent::MNone:
        switch (e.key())
        {
        case KeyEvent::KDelete:
            textBuffer_->del(cursor_);
            setCursor(cursor_);
            textBuffer_->render(this);
            break;
        case KeyEvent::KBackspace:
            textBuffer_->backspace(cursor_);
            setCursor(cursor_);
            textBuffer_->render(this);
            break;
        case KeyEvent::KReturn:
            textBuffer_->insert(cursor_, L"\n");
            setCursor(cursor_);
            textBuffer_->render(this);
            break;
        case KeyEvent::KLeft:
            moveCursor(&Screen::moveCursorLeft);
            break;
        case KeyEvent::KRight:
            moveCursor(&Screen::moveCursorRight);
            break;
        case KeyEvent::KUp:
            moveCursor(&Screen::moveCursorUp);
            break;
        case KeyEvent::KDown:
            moveCursor(&Screen::moveCursorDown);
            break;
        case KeyEvent::KHome:
            moveCursor(&Screen::moveCursorHome);
            break;
        case KeyEvent::KEnd:
            moveCursor(&Screen::moveCursorEnd);
            break;
        case KeyEvent::KPageUp:
            moveCursor(&Screen::moveCursorPageUp);
            break;
        case KeyEvent::KPageDown:
            moveCursor(&Screen::moveCursorPageDown);
            break;
        default:
            break;
        };
        break;
    case KeyEvent::MLCtrl:
    case KeyEvent::MRCtrl:
        switch (e.key())
        {
        case KeyEvent::KHome:
            setCursor(0, 0);
            textBuffer_->render(this);
            break;
        case KeyEvent::KEnd:
            setCursor((*textBuffer_)[textBuffer_->size() - 1].size(), textBuffer_->size() - 1);
            textBuffer_->render(this);
            break;
        case KeyEvent::KV:
            paste();
            textBuffer_->render(this);
            break;
        default:
            break;
        }
        break;
    case KeyEvent::MLShift:
    case KeyEvent::MRShift:
        switch (e.key())
        {
        case KeyEvent::KLeft:
            select(&Screen::moveCursorLeft);
            break;
        case KeyEvent::KRight:
            select(&Screen::moveCursorRight);
            break;
        case KeyEvent::KUp:
            select(&Screen::moveCursorUp);
            break;
        case KeyEvent::KDown:
            select(&Screen::moveCursorDown);
            break;
        case KeyEvent::KHome:
            select(&Screen::moveCursorHome);
            break;
        case KeyEvent::KEnd:
            select(&Screen::moveCursorEnd);
            break;
        case KeyEvent::KPageUp:
            select(&Screen::moveCursorPageUp);
            break;
        case KeyEvent::KPageDown:
            select(&Screen::moveCursorPageDown);
            break;
        case KeyEvent::KInsert:
            paste();
            textBuffer_->render(this);
            break;
        default:
            break;
        };
        break;
    }
    
    return true;
}

bool Screen::textInputEvent(TextInputEvent &e)
{
    if (textBuffer_)
    {
        textBuffer_->insert(cursor_, e.text());
        setCursor(cursor_);
        textBuffer_->render(this);
        return true;
    }
    return false;
}

void Screen::resizeEvent(ResizeEvent &e)
{
    ch_.resize(heightCh());
    for (auto &r: ch_)
        r.resize(widthCh());
}

int Screen::widthCh() const
{
    return width() / glyphWidth_;
}

int Screen::heightCh() const
{
    return height() / glyphHeight_;
}

Screen::Char &Screen::ch(int x, int y)
{
    return ch_[y][x];
}

const Screen::Char &Screen::ch(int x, int y) const
{
    return ch_[y][x];
}

Coord Screen::cursor() const
{
    return cursor_;
}

void Screen::setCursor(Coord value)
{
    if (value.y < vScroll_)
    {
        vScroll_ = value.y;
        value.y = vScroll_;
    }
    if (value.y > vScroll_ + heightCh() - 1)
    {
        vScroll_ = value.y - heightCh() + 1;
        value.y = vScroll_ + heightCh() - 1;
    }
    cursor_ = value;
}

void Screen::setCursor(int x, int y)
{
    setCursor(Coord{ x, y });
}

BaseTextBuffer *Screen::textBuffer() const
{
    return textBuffer_;
}

void Screen::setTextBuffer(BaseTextBuffer *value)
{
    if (value != textBuffer_)
    {
        textBuffer_ = value;
        if (textBuffer_)
            textBuffer_->render(this);
        else
        {
            for (size_t y = 0; y < ch_.size(); ++y)
                for (size_t x = 0; x < ch_[y].size(); ++x)
                    ch_[y][x] = '\0';
            update();
        }
    }
}

int Screen::hScroll() const
{
    return hScroll_;
}

void Screen::setHScroll(int value)
{
    hScroll_ = value;
    if (textBuffer_)
        textBuffer_->render(this);
}

int Screen::vScroll() const
{
    return vScroll_;
}

void Screen::setVScroll(int value)
{
    vScroll_ = value;
    if (textBuffer_)
        textBuffer_->render(this);
}

Coord Screen::startSelection() const
{
    return startSelection_;
}

void Screen::setStartSelection(Coord value)
{
    startSelection_ = value;
}

Coord Screen::endSelection() const
{
    return endSelection_;
}

void Screen::setEndSelection(Coord value)
{
    endSelection_ = value;
}

bool Screen::isSelected(Coord value) const
{
    Coord s, e;
    if (startSelection().y > endSelection().y ||
        (startSelection().y == endSelection().y && startSelection().x > endSelection().x))
    {
        s = endSelection();
        e = startSelection();
    }
    else
    {
        s = startSelection();
        e = endSelection();
    }

    if (s.x == -1 ||
        s.y == -1 ||
        e.x == -1 ||
        e.y == -1)
        return false;
    return (value.y > s.y && value.y < e.y) ||
        (value.y == s.y && value.y != e.y && value.x >= s.x) ||
        (value.y == e.y && value.y != s.y && value.x < e.x) ||
        (value.y == s.y && value.y != e.y && value.x >= s.x) ||
        (value.y == e.y && value.y == s.y && value.x < e.x && value.x >= s.x);
}

void Screen::moveCursorLeft()
{
    if (cursor_.x > 0)
        --cursor_.x;
    else
    {
        if (cursor_.y > 0)
        {
            --cursor_.y;
            cursor_.x = (*textBuffer_)[cursor_.y].size();
        }
    }
    setCursor(cursor_);
}

void Screen::moveCursorRight()
{
    if (cursor_.x < static_cast<int>((*textBuffer_)[cursor_.y].size()))
        ++cursor_.x;
    else
    {
        if (cursor_.y < textBuffer_->size() - 1)
        {
            ++cursor_.y;
            cursor_.x = 0;
        }
    }
    setCursor(cursor_);
}

void Screen::moveCursorUp()
{
    if (cursor_.y > 0)
    {
        --cursor_.y;
        if (cursor_.x > static_cast<int>((*textBuffer_)[cursor_.y].size()))
            cursor_.x = (*textBuffer_)[cursor_.y].size();
    }
    setCursor(cursor_);
}

void Screen::moveCursorDown()
{
    if (cursor_.y < textBuffer_->size() - 1)
    {
        ++cursor_.y;
        if (cursor_.x > static_cast<int>((*textBuffer_)[cursor_.y].size()))
            cursor_.x = (*textBuffer_)[cursor_.y].size();
    }
    setCursor(cursor_);
}

void Screen::moveCursorHome()
{
    cursor_.x = 0;
    setCursor(cursor_);
}

void Screen::moveCursorEnd()
{
    cursor_.x = (*textBuffer_)[cursor_.y].size();
    setCursor(cursor_);
}

void Screen::moveCursorPageUp()
{
    vScroll_ -= heightCh() - 1;
    if (vScroll_ < 0)
        vScroll_ = 0;
    cursor_.y -= heightCh() - 1;
    if (cursor_.y < vScroll_)
        cursor_.y = vScroll_;
    else if (cursor_.y >= vScroll_ + heightCh())
        cursor_.y = vScroll_ + heightCh() - 1;
    if (cursor_.y >= textBuffer_->size())
        cursor_.y = textBuffer_->size() - 1;
    if (cursor_.y < 0)
        cursor_.y = 0;
    if (cursor_.x > static_cast<int>((*textBuffer_)[cursor_.y].size()))
        cursor_.x = (*textBuffer_)[cursor_.y].size();
    setCursor(cursor_);
}

void Screen::moveCursorPageDown()
{
    vScroll_ += heightCh() - 1;
    if (vScroll_ >= textBuffer_->size() - 1)
        vScroll_ = textBuffer_->size() - 2;
    cursor_.y += heightCh() - 1;
    if (cursor_.y < vScroll_)
        cursor_.y = vScroll_;
    else if (cursor_.y >= vScroll_ + heightCh())
        cursor_.y = vScroll_ + heightCh() - 1;
    if (cursor_.y >= textBuffer_->size())
        cursor_.y = textBuffer_->size() - 1;
    if (cursor_.y < 0)
        cursor_.y = 0;
    if (cursor_.x > static_cast<int>((*textBuffer_)[cursor_.y].size()))
        cursor_.x = (*textBuffer_)[cursor_.y].size();
    setCursor(cursor_);
}

void Screen::select(void (Screen::*moveCursor)())
{
    if (startSelection().x == -1)
        setStartSelection(cursor());
    (this->*moveCursor)();
    setEndSelection(cursor());
    textBuffer_->render(this);
}

void Screen::moveCursor(void (Screen::*moveCursor)())
{
    setStartSelection({-1, -1});
    setEndSelection({-1, -1});
    (this->*moveCursor)();
    textBuffer_->render(this);
}

void Screen::copy()
{
    
}

void Screen::paste()
{
    std::clog << SDL_GetClipboardText() << std::endl;
    textBuffer_->insert(cursor_, toUtf16(SDL_GetClipboardText()));
    setCursor(cursor_);
}

void Screen::cut()
{
    
}