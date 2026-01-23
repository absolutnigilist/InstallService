#pragma once
#include "Cli.hpp"

namespace svcinst {

//---Оркестратор: запуск установщика службы с заданными опциями
int runInstaller(const CliOptions& options);

};//---namespace svcinst