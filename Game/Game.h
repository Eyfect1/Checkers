#pragma once
#include <chrono>
#include <thread>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

class Game
{
  public:
    Game() : board(config("WindowSize", "Width"), config("WindowSize", "Hight")), hand(&board), logic(&board, &config)
    {
        ofstream fout(project_path + "log.txt", ios_base::trunc);
        fout.close();
    }

    // to start checkers
    int play()
    {
        // Засекаем время начала партии
        auto start = chrono::steady_clock::now();
        // Если выбран режим повтора партии
        if (is_replay)
        {
            logic = Logic(&board, &config); // пересоздаём объект логики
            config.reload();                // перечитываем настройки
            board.redraw();                 // перерисовываем доску
        }
        else
        {
            board.start_draw();             // начальная отрисовка доски
        }
        is_replay = false;

        int turn_num = -1;
        bool is_quit = false;
        const int Max_turns = config("Game", "MaxNumTurns"); // максимальное число ходов
        // Основной игровой цикл
        while (++turn_num < Max_turns)
        {
            beat_series = 0; // сбрасываем серию взятий
            logic.find_turns(turn_num % 2); // ищем возможные ходы для текущего игрока
            if (logic.turns.empty())        // если ходов нет — конец игры
                break;
            // Устанавливаем уровень сложности бота для текущего цвета
            logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + string("BotLevel"));
            // Если ходит человек
            if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + string("Bot")))
            {
                auto resp = player_turn(turn_num % 2); // обработка хода игрока
                if (resp == Response::QUIT)
                {
                    is_quit = true; // игрок выбрал выход
                    break;
                }
                else if (resp == Response::REPLAY)
                {
                    is_replay = true; // игрок выбрал повтор партии
                    break;
                }
                else if (resp == Response::BACK)
                {
                    // Откат ходов, если выбран возврат
                    if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + string("Bot")) &&
                        !beat_series && board.history_mtx.size() > 2)
                    {
                        board.rollback();
                        --turn_num;
                    }
                    if (!beat_series)
                        --turn_num;

                    board.rollback();
                    --turn_num;
                    beat_series = 0;
                }
            }
            else
                bot_turn(turn_num % 2); // если ходит бот
        }
        // Засекаем время окончания партии
        auto end = chrono::steady_clock::now();
        // Записываем время игры в лог
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();

        // Если был выбран повтор — запускаем партию заново
        if (is_replay)
            return play();
        // Если был выход — возвращаем 0
        if (is_quit)
            return 0;
        int res = 2; // результат партии: 0 — ничья, 1 — победа чёрных, 2 — победа белых
        if (turn_num == Max_turns)
        {
            res = 0; // ничья по лимиту ходов
        }
        else if (turn_num % 2)
        {
            res = 1; // победа чёрных
        }
        board.show_final(res); // показываем финальный экран
        auto resp = hand.wait(); // ждём действия игрока (например, повтор)
        if (resp == Response::REPLAY)
        {
            is_replay = true;
            return play();
        }
        return res; // возвращаем результат партии
    }

  private:
    void bot_turn(const bool color)
    {
        // Засекаем время начала хода бота
        auto start = chrono::steady_clock::now();

        auto delay_ms = config("Bot", "BotDelayMS");
        // Запускаем отдельный поток для задержки (имитация раздумий бота)
        thread th(SDL_Delay, delay_ms);
        auto turns = logic.find_best_turns(color); // Получаем лучший(ие) ход(ы) для бота
        th.join(); // Дожидаемся завершения задержки
        bool is_first = true;
        // Выполняем все ходы из найденной последовательности
        for (auto turn : turns)
        {
            if (!is_first)
            {
                SDL_Delay(delay_ms); // Задержка между последовательными ходами (например, при серии взятий)
            }
            is_first = false;
            beat_series += (turn.xb != -1); // Увеличиваем счётчик серии взятий, если был побит противник
            board.move_piece(turn, beat_series); // Выполняем ход на доске
        }

        // Засекаем время окончания хода бота
        auto end = chrono::steady_clock::now();
        // Записываем время хода бота в лог
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Bot turn time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();
    }

    Response player_turn(const bool color)
    {
        // Формируем список клеток, доступных для хода
        vector<pair<POS_T, POS_T>> cells;
        for (auto turn : logic.turns)
        {
            cells.emplace_back(turn.x, turn.y);
        }
        board.highlight_cells(cells); // Подсвечиваем возможные клетки для хода
        move_pos pos = {-1, -1, -1, -1};
        POS_T x = -1, y = -1;
        // Цикл выбора первой клетки (фигуры для хода)
        while (true)
        {
            auto resp = hand.get_cell(); // Получаем действие игрока
            if (get<0>(resp) != Response::CELL)
                return get<0>(resp); // Если не клетка — возвращаем результат (выход, откат и т.д.)
            pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};

            bool is_correct = false;
            for (auto turn : logic.turns)
            {
                if (turn.x == cell.first && turn.y == cell.second)
                {
                    is_correct = true;
                    break;
                }
                if (turn == move_pos{x, y, cell.first, cell.second})
                {
                    pos = turn;
                    break;
                }
            }
            if (pos.x != -1)
                break; // выбран ход
            if (!is_correct)
            {
                if (x != -1)
                {
                    board.clear_active();
                    board.clear_highlight();
                    board.highlight_cells(cells);
                }
                x = -1;
                y = -1;
                continue; // некорректный выбор — повторяем
            }
            x = cell.first;
            y = cell.second;
            board.clear_highlight();
            board.set_active(x, y); // выделяем выбранную фигуру
            vector<pair<POS_T, POS_T>> cells2;
            for (auto turn : logic.turns)
            {
                if (turn.x == x && turn.y == y)
                {
                    cells2.emplace_back(turn.x2, turn.y2);
                }
            }
            board.highlight_cells(cells2); // подсвечиваем возможные клетки для хода выбранной фигурой
        }
        board.clear_highlight();
        board.clear_active();
        board.move_piece(pos, pos.xb != -1); // выполняем ход
        if (pos.xb == -1)
            return Response::OK; // если не было взятия — ход завершён
        // Продолжаем серию взятий, если это возможно
        beat_series = 1;
        while (true)
        {
            logic.find_turns(pos.x2, pos.y2); // ищем возможные взятия с новой позиции
            if (!logic.have_beats)
                break; // если больше нет взятий — серия завершена

            vector<pair<POS_T, POS_T>> cells;
            for (auto turn : logic.turns)
            {
                cells.emplace_back(turn.x2, turn.y2);
            }
            board.highlight_cells(cells); // подсвечиваем возможные клетки для следующего взятия
            board.set_active(pos.x2, pos.y2); // выделяем текущую фигуру
            // Цикл выбора следующего взятия
            while (true)
            {
                auto resp = hand.get_cell();
                if (get<0>(resp) != Response::CELL)
                    return get<0>(resp); // если не клетка — возвращаем результат
                pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};

                bool is_correct = false;
                for (auto turn : logic.turns)
                {
                    if (turn.x2 == cell.first && turn.y2 == cell.second)
                    {
                        is_correct = true;
                        pos = turn;
                        break;
                    }
                }
                if (!is_correct)
                    continue; // некорректный выбор — повторяем

                board.clear_highlight();
                board.clear_active();
                beat_series += 1;
                board.move_piece(pos, beat_series); // выполняем взятие
                break;
            }
        }

        return Response::OK; // ход игрока завершён
    }

  private:
    Config config;
    Board board;
    Hand hand;
    Logic logic;
    int beat_series;
    bool is_replay = false;
};
