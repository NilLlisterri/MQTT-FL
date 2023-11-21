#include "flCommandService.h"
#include "fl.h"

FlCommandService::FlCommandService() {
    // addCommand(Command("/flGetWeights", "Get the weights for the specified batch", FlCommand::GetWeights, 1, [this](String args) {
    //     return String(Fl::getInstance().flGetWeights(strtol(args.c_str(), NULL, 16)));
    // }));
}
