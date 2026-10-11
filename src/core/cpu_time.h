// Процессорное время (пользовательское + системное) для замеров нагрузки: одинаково на macOS, Linux и Windows.
#pragma once

namespace cpu_time {

// Вызывающего потока, секунд
double thread();
// Всего процесса, все потоки, секунд
double process();

}  // namespace cpu_time
