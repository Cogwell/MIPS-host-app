#include "ringbuffer.h"

#include <QDebug>


RingBuffer::RingBuffer(void)
{
    clear();
}

void RingBuffer::clear(void)
{
    head = 0;
    tail = 0;
    count = 0;
    lines = 0;
}

void RingBuffer::waitforline(int timeout = 0)
{
    QTime timer;

    if(timeout == 0)
    {
        while(1)
        {
            QApplication::processEvents();
            if(lines > 0) break;
        }
        return;
    }
    timer.start();
    while(timer.elapsed() < timeout)
    {
        QApplication::processEvents();
        if(lines > 0)
        {
            break;
        }
    }
}

int RingBuffer::size(void)
{
    return count;
}

char RingBuffer::getch(void)
{
    char c;

    if(count == 0) return(0);
    c = buffer[tail++];
    if(tail >= SIZE) tail = 0;
    count--;
    if(c == '\n')
    {
        lines--;
    }
    return c;
}

int RingBuffer::putch(char c)
{
    if(c == 0x06) return(count);
    if(c == 0x15) return(count);
    if(c == '\r') return(count);        // ignore \r
    if(count >= SIZE) return(-1);
    if(c == '\n')
    {
        lines++;
    }
    buffer[head++] = c;
    if(head >= SIZE) head = 0;
    count++;
    return(count);
}

QString RingBuffer::getline(void)
{
    QString str="";
    char c;

    if(lines <= 0) return str;
    while(1)
    {
        c = getch();
        if(c == '\n') break;
        if(count <= 0) break;
        str += c;
    }
    return str;
}
