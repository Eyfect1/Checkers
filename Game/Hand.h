#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

// Класс для обработки пользовательского ввода ("рука" игрока)
class Hand
{
  public:
    Hand(Board *board) : board(board)
    {
    }
    // Ожидает выбор клетки игроком или другое действие (выход, откат, повтор)
    tuple<Response, POS_T, POS_T> get_cell() const
    {
        SDL_Event windowEvent;
        Response resp = Response::OK;
        int x = -1, y = -1;
        int xc = -1, yc = -1;
        while (true)
        {
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:
                    resp = Response::QUIT; // Игрок закрыл окно
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    x = windowEvent.motion.x;
                    y = windowEvent.motion.y;
                    // Преобразуем координаты мыши в координаты клетки доски
                    xc = int(y / (board->H / 10) - 1);
                    yc = int(x / (board->W / 10) - 1);
                    // Если клик по области "назад" и есть история ходов
                    if (xc == -1 && yc == -1 && board->history_mtx.size() > 1)
                    {
                        resp = Response::BACK;
                    }
                    // Если клик по области "повтор"
                    else if (xc == -1 && yc == 8)
                    {
                        resp = Response::REPLAY;
                    }
                    // Если клик по игровой клетке
                    else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8)
                    {
                        resp = Response::CELL;
                    }
                    // Клик вне допустимых областей
                    else
                    {
                        xc = -1;
                        yc = -1;
                    }
                    break;
                case SDL_WINDOWEVENT:
                    if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        board->reset_window_size(); // Обработка изменения размера окна
                        break;
                    }
                }
                if (resp != Response::OK)
                    break; // Если получено действие — выходим из цикла
            }
        }
        return {resp, xc, yc}; // Возвращаем тип действия и координаты клетки
    }

    // Ожидает одно из действий: выход или повтор партии
    Response wait() const
    {
        SDL_Event windowEvent;
        Response resp = Response::OK;
        while (true)
        {
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:
                    resp = Response::QUIT; // Игрок закрыл окно
                    break;
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    board->reset_window_size(); // Обработка изменения размера окна
                    break;
                case SDL_MOUSEBUTTONDOWN: {
                    int x = windowEvent.motion.x;
                    int y = windowEvent.motion.y;
                    int xc = int(y / (board->H / 10) - 1);
                    int yc = int(x / (board->W / 10) - 1);
                    // Если клик по области "повтор"
                    if (xc == -1 && yc == 8)
                        resp = Response::REPLAY;
                }
                break;
                }
                if (resp != Response::OK)
                    break; // Если получено действие — выходим из цикла
            }
        }
        return resp; // Возвращаем тип действия
    }

  private:
    Board *board; // Указатель на игровую доску
};
