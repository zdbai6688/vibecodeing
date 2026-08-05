head_lines=70, tail_lines=378
HEAD_START:
// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: 
TAIL_START:
{
    QDate d = m_currentMonday.addDays(dayIndex);
    m_selectedDate = d;
    updateDayTodoList(d);
