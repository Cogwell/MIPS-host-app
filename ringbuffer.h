#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <QByteArray>
#include <QString>
#include <QTime>
#include <QApplication>

#define SIZE 10000

class RingBuffer
{
//    Q_OBJECT

public:
    explicit RingBuffer(void);
    char getch(void);
    int  putch(char c);
    int  size(void);
    void clear(void);
    void waitforline(int);
    QString getline(void);

protected:

private:
    int     buffer[SIZE];
    int     head;
    int     tail;
    int     count;
    int     lines;
};

#endif // RINGBUFFER_H

