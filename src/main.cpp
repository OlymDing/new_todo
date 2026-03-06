#include "config.h"
#include "db.h"
#include "cli/cli_app.h"

int main(int argc, char** argv) {
    AppConfig cfg;
    try { cfg = ConfigLoader::load("config.json"); }
    catch (...) { cfg = ConfigLoader::defaults(); }

    Database    db(cfg.db_path);
    cli::CliApp app(db, cfg);
    return app.run(argc, argv);
}
