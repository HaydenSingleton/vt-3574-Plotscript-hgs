#include "notebook_app.hpp"

NotebookApp::NotebookApp(QWidget *parent) : QWidget(parent) {
    setObjectName("notebook");
    in = new InputWidget(this); //child widgets of notebook
    out = new OutputWidget(this);

    auto layout = new QVBoxLayout(this); // add the widgets to a vertical layout
    layout->addWidget(in, 1);
    layout->addWidget(out, 1);
    setLayout(layout);

    // connect input to this notebook for evaluating
    QObject::connect(in, SIGNAL(send_input(QString)), this, SLOT(catch_input(QString)));

    // sent evaluated expressions to output widget
    QObject::connect(this, SIGNAL(send_result(Expression)), out, SLOT(catch_result(Expression)));

    // send any errors to output widget with a different mechanism
    QObject::connect(this, SIGNAL(send_failure(int, std::string)), out, SLOT(catch_failure(int, std::string)));

    QObject::connect(in, SIGNAL(clear_output()), out, SLOT(clear_screen()));

    //startup procedure
    std::ifstream startip_str(STARTUP_FILE);
    if(!mrInterpret.parseStream(startip_str)){
        emit send_failure(1, "Invalid Program. Could not parse.");
    }
    else{
        try{
            Expression exp = mrInterpret.evaluate();
        }
        catch(const SemanticError & ex){
            emit send_failure(1, ex.what());
        }
    }
}

void NotebookApp::catch_input(QString r){
    std::istringstream expression(r.toStdString());
    if(!mrInterpret.parseStream(expression)){
        emit send_failure(0, "Invalid Expression. Could not parse.");
    }
    else{
        try{
            Expression exp = mrInterpret.evaluate();
            emit send_result(exp);
        }
        catch(const SemanticError & ex){
            emit send_failure(0, ex.what());
        }
    }
}