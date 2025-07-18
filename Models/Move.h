#pragma once
#include <stdlib.h>

// Тип для хранения координат на доске (от -128 до 127)
typedef int8_t POS_T;

// Структура, описывающая ход шашки
struct move_pos
{
    POS_T x, y;             // координаты начальной клетки (откуда)
    POS_T x2, y2;           // координаты конечной клетки (куда)
    POS_T xb = -1, yb = -1; // координаты побитой шашки (если есть), -1 если взятия нет

    // Конструктор для обычного хода (без взятия)
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2) : x(x), y(y), x2(x2), y2(y2)
    {
    }
    // Конструктор для хода с взятием шашки
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2, const POS_T xb, const POS_T yb)
        : x(x), y(y), x2(x2), y2(y2), xb(xb), yb(yb)
    {
    }

    // Оператор сравнения: равны, если совпадают начальные и конечные координаты
    bool operator==(const move_pos &other) const
    {
        return (x == other.x && y == other.y && x2 == other.x2 && y2 == other.y2);
    }
    // Оператор неравенства
    bool operator!=(const move_pos &other) const
    {
        return !(*this == other);
    }
};
