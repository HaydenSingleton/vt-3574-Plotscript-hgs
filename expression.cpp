#include "expression.hpp"

#include <sstream>
#include <list>

#include "environment.hpp"
#include "semantic_error.hpp"

Expression::Expression(): m_type(ExpType::Empty)
{}

// Basic constructor
Expression::Expression(const Atom & a): m_head(a), m_type(ExpType::None)
{}

// recursive copy
Expression::Expression(const Expression & a) {

  m_head = a.m_head;

  m_type = a.m_type;
  m_properties = a.m_properties;
  
  for(auto e : a.m_tail){
    m_tail.push_back(e);
  }
}

// Constructor for lists
Expression::Expression(const std::vector<Expression> & items): m_tail(items) {
  m_type = ExpType::List;
}

//Constructor for Lambda functions
Expression::Expression(const std::vector<Expression> & args, Expression & func) {

  m_type = ExpType::Lambda;

  m_tail.push_back(args);
  m_tail.push_back(func);
}

// Constructor for graphics items
Expression::Expression(const Atom & t, const std::vector<Expression> & back){

  for(auto e : back){
    m_tail.push_back(e);
  }
  
}

// Constructor for plots
Expression::Expression(std::string type, const std::vector<Expression> & back) {

  if (type == "CP") {
    m_type = ExpType::CP;
  }
  else if (type == "DP") {
    m_type == ExpType::DP;
  }

  m_head = Atom(type);

  for(auto e : back){
    m_tail.push_back(e);
  }

}

Expression & Expression::operator=(const Expression & a){

  // prevent self-assignment
  if(this != &a){
      m_type = a.m_type;
      m_head = a.m_head;
      m_properties = a.m_properties;
      m_tail.clear();
      for(auto e : a.m_tail)
        m_tail.push_back(e);
  }

  return *this;
}


Atom & Expression::head(){
  return m_head;
}

const Atom & Expression::head() const{
  return m_head;
}

bool Expression::isNone() const noexcept {
	return (m_type == ExpType::None);
}

bool Expression::isList() const noexcept {
	return (m_type == ExpType::List);
}

bool Expression::isLambda() const noexcept {
  return (m_type == ExpType::Lambda);
}

bool Expression::isEmpty() const noexcept {
  return (m_type == ExpType::Empty);
}

bool Expression::isDP() const noexcept {
  return m_type == ExpType::DP;
}

bool Expression::isCP() const noexcept {
  return m_type == ExpType::CP;
}

void Expression::append(const Atom & a){
  m_tail.emplace_back(a);
}

Expression * Expression::tail(){
  Expression * ptr = nullptr;

  if(m_tail.size() > 0){
    ptr = &m_tail.back();
  }

  return ptr;
}

size_t Expression::tailLength() const noexcept{
  return m_tail.size();
}

Expression::ConstIteratorType Expression::tailConstBegin() const noexcept{
  return m_tail.cbegin();
}

Expression::ConstIteratorType Expression::tailConstEnd() const noexcept{
  return m_tail.cend();
}

Expression apply(const Atom & op, const std::vector<Expression> & args, const Environment & env){

  if ( env.get_exp(op).isLambda() ) {
    Environment inner_scope = env;

    Expression lambda = inner_scope.get_exp(op);
    Expression arg_template = *lambda.tailConstBegin();

    if(args.size() != arg_template.tailLength()){
      throw SemanticError("Error: during apply: Error in call to procedure: invalid number of arguments.");
    }

    size_t count = 0;
    for(auto p = arg_template.tailConstBegin(); p != arg_template.tailConstEnd(); p++){
      inner_scope.__shadowing_helper(p->head(), args[count++]);
    }

    return lambda.tail()->eval(inner_scope);
  }

  // head must be a symbol
  if(!op.isSymbol()){
    throw SemanticError("Error during evaluation: procedure "+op.asString()+" is not a symbol");
  }

  // must map to a proc
  if(!env.is_proc(op)){
    throw SemanticError("Error during evaluation: symbol "+op.asString()+" does not name a procedure");
  }

  // map from symbol to proc
  Procedure proc = env.get_proc(op);

  // call proc with args
  return proc(args);
}

Expression Expression::handle_lookup(const Atom & head, const Environment & env){

    if(head.isSymbol()) { // if symbol is in env return value
      if(env.is_exp(head)) {
	      return env.get_exp(head);
      }
      else {
	      throw SemanticError("Error during handle lookup: unknown symbol " + head.asSymbol());
      }
    }
    else if(head.isNumber() || head.isComplex() || head.isString()){
      return Expression(head);
    }
    else{
      throw SemanticError("Error during handle lookup: Invalid type in terminal expression");
    }
}

Expression Expression::handle_begin(Environment & env){

  Expression result;
  for(Expression::IteratorType it = m_tail.begin(); it != m_tail.end(); ++it){
    result = it->eval(env);
  }

  // go through a whole "program" and return the last result
  return result;
}

Expression Expression::handle_define(Environment & env){

  // check expected tail size
  if(m_tail.size() != 2){
    throw SemanticError("Error during handle define: invalid number of arguments to define");
  }

  // tail[0] must be symbol
  if(!m_tail[0].head().isSymbol()){
    throw SemanticError("Error during handle define: first argument to define not symbol");
  }

  // but tail[0] must not be a special-form or procedure
  std::string s = m_tail[0].head().asSymbol();
  if((s == "define") || (s == "begin") || (s == "lambda") || (s == "list")) {
    throw SemanticError("Error during handle define: attempt to redefine a special-form");
  }
  else if(env.is_proc(m_tail[0].head())) {
    throw SemanticError("Error during handle define: attempt to redefine a built-in procedure");
  }
  else if((s=="pi"||(s=="e")||s=="I")) {
    throw SemanticError("Error during handle define: attempt to redefine a built-in symbol");
  }
  else {
    // eval tail[1]
    Expression result = m_tail[1].eval(env);

    //and add to env
    env.add_exp(m_tail[0].head(), result);

    return result;
  }
}

Expression Expression::handle_list(Environment & env){

  std::vector<Expression> listItems;
  for(auto e = m_tail.begin(); e != m_tail.end(); e++){
    listItems.push_back(e->eval(env));
  }

  return Expression(listItems);
}

Expression Expression::handle_lambda(Environment & env) {

  std::vector<Expression> argument_template;
  argument_template.emplace_back(Expression(m_tail[0].head()));
  for(auto e = m_tail[0].tailConstBegin(); e!=m_tail[0].tailConstEnd(); e++){
    argument_template.emplace_back(Expression(*e));
  }

  Expression return_exp = Expression(argument_template, m_tail[1]);
  return return_exp;
}

Expression Expression::handle_apply(Environment & env){

  if(m_tail.size() != 2){
    throw SemanticError("Error during apply: invalid number of arguments");
  }

  Atom op =  m_tail[0].head();
  if ( env.get_exp(op).isLambda() ) {
  }
  else {
    if(!env.is_proc(op) || m_tail[0].tailLength() > 0){ 
      throw SemanticError("Error: first argument to apply not a procedure");
    }
  }

  Expression arguments = m_tail[1].eval(env);
  if(!arguments.isList()){
    throw SemanticError("Error: second argument to apply not a list");
  }

  std::vector<Expression> list_args;
  for(auto e = arguments.tailConstBegin(); e != arguments.tailConstEnd(); e++){
    list_args.push_back(*e);
  }

  return apply(op, list_args, env);
}

Expression Expression::handle_map(Environment & env){

  if(m_tail.size() != 2){
    throw SemanticError("Error during map: invalid number of arguments");
  }

  Atom op =  m_tail[0].head();
  if ( env.get_exp(op).head().asSymbol() == "Lambda" ) {
  }
  else {
    if(!env.is_proc(op) || m_tail[0].tailLength() > 0){ 
      throw SemanticError("Error: first argument to map not a procedure");
    }
  }


  Expression list_evaled = m_tail[1].eval(env);
  if(!list_evaled.isList()){
    throw SemanticError("Error: second argument to apply not a list");
  }

  std::vector<Expression> return_args;
  std::vector<Expression> temp;
  Expression temp_e;

  for(auto e = list_evaled.tailConstBegin(); e != list_evaled.tailConstEnd(); e++){
    temp.emplace_back(*e);
    temp_e = apply(m_tail[0].head(), temp, env);
    return_args.push_back(temp_e);
    temp.clear();
  }

  return Expression(return_args);
}

Expression Expression::handle_set_property(Environment & env) {

  Expression result;

   if(m_tail.size()==3) {
    if(m_tail[0].head().isString()) {

      result = m_tail[2].eval(env);
      std::string key = m_tail[0].head().asString();
      if(result.m_properties.find(key) != result.m_properties.end()){
        result.m_properties.erase(key);
      }
      Expression value = m_tail[1].eval(env);
      result.m_properties[key] = value;
    }
    else{
      throw SemanticError("Error: first argument to set-property not a string.");
    }
  }
  else{
    throw SemanticError("Error invalid number of arguments for set-property.");
  }

  return result;
}

Expression Expression::handle_get_property(Environment & env){
  Expression target, result;
  if(m_tail.size()==2) {
    target = m_tail[1].eval(env);
    if(m_tail[0].head().isString()){
      std::string key = m_tail[0].head().asString();
      if(target.m_properties.find(key)!= target.m_properties.end()){
        result = target.m_properties.at(key);
      }
      else {
        result.m_type = ExpType::Empty;
      }
      return result;
    }
    else{
      throw SemanticError("Error: first argument to get-property not a string.");
    }
  }
  else{
    throw SemanticError("Error: invalid number of arguments for get-property.");
  }
}

Expression Expression::handle_discrete_plot(Environment & env){
  if(m_tail.size()==2){
    std::vector<Expression> result;
    Expression DATA = m_tail[0].eval(env);
    Expression OPTIONS = m_tail[1].eval(env);
    if(DATA.isList() && OPTIONS.isList()) {

      // Constants used to create bounding box and graph
      double N = 20;

      // Find the max and min values of x and y inside DATA
      double xmax = -999, xmin = 999, ymax = -999, ymin = 999;

      for(auto & point : DATA.m_tail){
        if(point.m_tail[0].head().asNumber() > xmax)
          xmax = point.m_tail[0].head().asNumber();
        if(point.m_tail[0].head().asNumber() < xmin)
          xmin = point.m_tail[0].head().asNumber();

        if(point.m_tail[1].head().asNumber() > ymax)
          ymax = point.m_tail[1].head().asNumber();
        if(point.m_tail[1].head().asNumber() < ymin)
          ymin = point.m_tail[1].head().asNumber();
      }
      // Create scale factors using the max and min edges of the data
      double xscale = N/(xmax-xmin), yscale = N/(ymax-ymin);
      double AL = xmin, AU = xmax, OL = ymin, OU = ymax;

      // Scale bounds of the box
      xmin *= xscale;
      ymin *= yscale * -1;
      xmax *= xscale;
      ymax *= yscale * -1;

      double xmiddle = (xmax+xmin)/2, ymiddle = (ymin-ymax)/2;

      // Make an expression for each point of the bounding box
      Expression topLeft, topMid, topRight, midLeft, midMid, midRight, botLeft, botMid, botRight;
      std::vector<Expression> xy = {Expression(xmin), Expression(ymax)};
      topLeft = Expression(Atom("make-point"), xy);

      xy = {Expression(xmiddle), Expression(ymax)};
      topMid = Expression(Atom("make-point"), xy);

      xy = {Expression(xmax), Expression(ymax)};
      topRight = Expression(Atom("make-point"), xy);

      xy = {Expression(xmin), Expression(ymiddle)};
      midLeft = Expression(Atom("make-point"), xy);

      xy = {Expression(xmiddle), Expression(ymiddle)};
      midMid = Expression(Atom("make-point"), xy);

      xy = {Expression(xmax), Expression(ymiddle)};
      midRight = Expression(Atom("make-point"), xy);

      xy = {Expression(xmin), Expression(ymin)};
      botLeft = Expression(Atom("make-point"), xy);

      xy = {Expression(xmiddle), Expression(ymin)};
      botMid = Expression(Atom("make-point"), xy);

      xy = {Expression(xmax), Expression(ymin)};
      botRight = Expression(Atom("make-point"), xy);

      // Make an expression to hold each line of the bounding rect
      Expression leftLine, topLine, rightLine, botLine, xaxis, yaxis;
      std::vector<Expression> newline;

      newline = {topLeft, botLeft};
      leftLine = Expression(Atom("make-line"), newline);
      result.push_back(leftLine.eval(env));

      newline = {topRight, botRight};
      rightLine = Expression(Atom("make-line"), newline);
      result.push_back(rightLine.eval(env));

      newline = {topLeft, topRight};
      topLine = Expression(Atom("make-line"), newline);
      result.push_back(topLine.eval(env));

      newline = {botLeft, botRight};
      botLine = Expression(Atom("make-line"), newline);
      result.push_back(botLine.eval(env));

      // Add draw axis lines if either zero line is within the boundaries
      if(0 < OU && 0 > OL){
        Expression xAxisStart, xAxisEnd;
        xy = {Expression(xmax), Expression(0.0)};
        xAxisStart = Expression(Atom("make-point"), xy);

        xy = {Expression(xmin), Expression(0.0)};
        xAxisEnd = Expression(Atom("make-point"), xy);

        newline = {xAxisStart, xAxisEnd};
        xaxis = Expression(Atom("make-line"), newline);
        result.push_back(xaxis.eval(env));
      }

      if(0 < AU && 0 > AL){
        Expression yAxisStart, yAxisEnd;
        xy = {Expression(0.0), Expression(ymax)};
        yAxisStart = Expression(Atom("make-point"), xy);

        xy = {Expression(0.0), Expression(ymin)};
        yAxisEnd = Expression(Atom("make-point"), xy);

        newline = {yAxisStart, yAxisEnd};
        yaxis = Expression(Atom("make-line"), newline);
        result.push_back(yaxis.eval(env));
      }

      // Add all data points and stem lines
      Expression new_point, stem_bottom, stemline; 
      double x,y; 
      std::vector<Expression> p1p2;

      /* If the bottom of the graph is above the orign,
      draw the stemlines down to the bottom line only */
      double stembottomy = std::max(0.0, OL)*yscale*-1;

      for(auto & point : DATA.m_tail){
        x = point.m_tail[0].head().asNumber() * xscale;
        y = point.m_tail[1].head().asNumber() * yscale * -1;

        xy = {Expression(x), Expression(y)};
        new_point = Expression(Atom("make-point"), xy);

        xy = {Expression(x), Expression(stembottomy)};
        stem_bottom = Expression(Atom("make-point"), xy);

        p1p2 = {new_point, stem_bottom};
        stemline = Expression(Atom("make-line"), p1p2);

        result.push_back(new_point.eval(env));
        result.push_back(stemline.eval(env));
      }

      // Add the bounds of the data as strings
      result.push_back(Expression(Atom("\""+Atom(AL).asString()+"\"")));
      result.push_back(Expression(Atom("\""+Atom(AU).asString()+"\"")));
      result.push_back(Expression(Atom("\""+Atom(OL).asString()+"\"")));
      result.push_back(Expression(Atom("\""+Atom(OU).asString()+"\"")));

      // Add each option to the output
      for(auto &opt : OPTIONS.m_tail){
        result.push_back(opt.m_tail[1]);
      }

      return Expression("DP", result);
    }
    else {
      throw SemanticError("Error: An argument to discrete-plot is not a list");
    }
  }
  else {
    throw SemanticError("Error: invalid number of arguments for discrete-plot");
  }
}

Expression Expression::handle_cont_plot(Environment & env){

    if(m_tail.size() == 2 || m_tail.size() == 3){
      std::vector<Expression> result;
      Expression FUNC = m_tail[0];
      Expression BOUNDS = m_tail[1];
      if(!FUNC.eval(env).isLambda()) {
        throw SemanticError("Error: first argument to continuous plot not a lambda");
      }
      else if(!BOUNDS.eval(env).isList()){
        throw SemanticError("Error: second argument to continuous plot not a list");
      }
      else if(m_tail.size() == 3 && !m_tail[2].eval(env).isList()){
        throw SemanticError("Error: third argument to continuous plot not a list");
      }

      std::vector<Expression> x_bounds, y_bounds, temp;
      x_bounds = BOUNDS.eval(env).m_tail;
      temp = {FUNC, BOUNDS};
      y_bounds = Expression(Atom("map"), temp).eval(env).m_tail;

      double N = 20, A = 3, B = 3, C = 2, D = 2;
      double xscale, yscale, AL, AU, OL, OU, xmin, xmax, ymin, ymax;

      AL = std::min(x_bounds.begin()->head().asNumber(), (x_bounds.end()-1)->head().asNumber());
      AU = std::max(x_bounds.begin()->head().asNumber(), (x_bounds.end()-1)->head().asNumber());

      OL = std::min(y_bounds.begin()->head().asNumber(), (y_bounds.end()-1)->head().asNumber());
      OU = std::max(y_bounds.begin()->head().asNumber(), (y_bounds.end()-1)->head().asNumber());

      // // Create scale factors using the max and min edges of the data
      xscale = N/(AU-AL);
      yscale = N/(OU-OL)  * -1;

      // // Scale bounds of the box
      xmin = AL * xscale;
      xmax = AU * xscale;
      ymin = OL * yscale;
      ymax = OU  * yscale;

      double xmiddle = (xmax+xmin)/2, ymiddle = (ymin+ymax)/2;

      // Make an expression for each point of the bounding box
      Expression topLeft, topMid, topRight, midLeft, midMid, midRight, botLeft, botMid, botRight;
      std::vector<Expression> xy = {Expression(xmin), Expression(ymax)};
      topLeft = Expression(Atom("make-point"), xy);

      xy = {Expression(xmiddle), Expression(ymax)};
      topMid = Expression(Atom("make-point"), xy);

      xy = {Expression(xmax), Expression(ymax)};
      topRight = Expression(Atom("make-point"), xy);

      xy = {Expression(xmin), Expression(ymiddle)};
      midLeft = Expression(Atom("make-point"), xy);

      xy = {Expression(xmiddle), Expression(ymiddle)};
      midMid = Expression(Atom("make-point"), xy);

      xy = {Expression(xmax), Expression(ymiddle)};
      midRight = Expression(Atom("make-point"), xy);

      xy = {Expression(xmin), Expression(ymin)};
      botLeft = Expression(Atom("make-point"), xy);

      xy = {Expression(xmiddle), Expression(ymin)};
      botMid = Expression(Atom("make-point"), xy);

      xy = {Expression(xmax), Expression(ymin)};
      botRight = Expression(Atom("make-point"), xy);

      // Make an expression to hold each line of the bounding rect
      Expression leftLine, topLine, rightLine, botLine, xaxis, yaxis;
      std::vector<Expression> newline;

      newline = {topLeft, botLeft};
      leftLine = Expression(Atom("make-line"), newline);
      result.push_back(leftLine.eval(env));

      newline = {topRight, botRight};
      rightLine = Expression(Atom("make-line"), newline);
      result.push_back(rightLine.eval(env));

      newline = {topLeft, topRight};
      topLine = Expression(Atom("make-line"), newline);
      result.push_back(topLine.eval(env));

      newline = {botLeft, botRight};
      botLine = Expression(Atom("make-line"), newline);
      result.push_back(botLine.eval(env));

      // Add draw axis lines if either zero line is within the boundaries
      if(0 < OU && 0 > OL){
        Expression xAxisStart, xAxisEnd;
        xy = {Expression(xmax), Expression(0.0)};
        xAxisStart = Expression(Atom("make-point"), xy);

        xy = {Expression(xmin), Expression(0.0)};
        xAxisEnd = Expression(Atom("make-point"), xy);

        newline = {xAxisStart, xAxisEnd};
        xaxis = Expression(Atom("make-line"), newline);
        result.push_back(xaxis.eval(env));
      }

      if(0 < AU && 0 > AL){
        Expression yAxisStart, yAxisEnd;
        xy = {Expression(Atom(0)), Expression(ymax)};
        yAxisStart = Expression(Atom("make-point"), xy);

        xy = {Expression(Atom(0)), Expression(ymin)};
        yAxisEnd = Expression(Atom("make-point"), xy);

        newline = {yAxisStart, yAxisEnd};
        yaxis = Expression(Atom("make-line"), newline);
        result.push_back(yaxis.eval(env));
      }

      // Number of data points for a continuous plot
      size_t M = 50;

      double stepsize = (AU-AL)/M;

      std::vector<double> domain, range;
      std::vector<Expression> domain_exp, range_exp;

      for(double x = AL; x <= AU+stepsize; x+=stepsize){
        domain.emplace_back(x);
        domain_exp.emplace_back(Expression(x));
      }

      temp = {FUNC, Expression(Atom("list"), domain_exp)};
      range_exp = Expression(Atom("map"), temp).eval(env).m_tail;

      Expression point;
      std::vector<Expression> points;
      size_t pos = 0;


      for(auto _ : range_exp){
        range.push_back(range_exp[pos].head().asNumber());
        pos++;
      }

      pos = 0;
      double scaled_X = 0, scaled_Y = 0;
      for(auto _ : range_exp){
        scaled_X = domain[pos] * xscale;
        scaled_Y = range[pos] * yscale;
        if(scaled_X < 0.001 && scaled_X > -0.001)
          scaled_X = 0.0;
        if(scaled_Y < 0.001 && scaled_Y > -0.001)
          scaled_Y = 0.0;
        temp = {Expression(scaled_X), Expression(scaled_Y)};
        point = Expression(Atom("make-point"), temp);
        points.push_back(point);
        pos++;
      }

      Expression line;
      for(size_t i = 1; i < points.size(); i++){

        temp = {points[i-1], points[i]};
        line = Expression(Atom("make-line"), temp);
        result.push_back(line.eval(env));
      }

      // Add the bounds of the data as strings for making labels
      Expression boundLabel, boundPos;
      temp = {Expression(Atom("\""+Atom(AL).asString()+"\""))};
      boundLabel = Expression(Atom("make-text"), temp).eval(env);
      temp = {Expression(xmin), Expression(ymin + C)};
      boundPos = Expression(Atom("make-point"), temp).eval(env);
      boundLabel.setTextPosition(boundPos);
      result.push_back(boundLabel);


      temp = {Expression(Atom("\""+Atom(AU).asString()+"\""))};
      boundLabel = Expression(Atom("make-text"), temp).eval(env);
      temp = {Expression(xmax), Expression(ymin + C)};
      boundPos = Expression(Atom("make-point"), temp).eval(env);
      boundLabel.setTextPosition(boundPos);
      result.push_back(boundLabel);

      temp = {Expression(Atom("\""+Atom(OL).asString()+"\""))};
      boundLabel = Expression(Atom("make-text"), temp).eval(env);
      temp = {Expression(xmin-D), Expression(ymin)};
      boundPos = Expression(Atom("make-point"), temp).eval(env);
      boundLabel.setTextPosition(boundPos);
      result.push_back(boundLabel);

      temp = {Expression(Atom("\""+Atom(OU).asString()+"\""))};
      boundLabel = Expression(Atom("make-text"), temp).eval(env);

      temp = {Expression(xmin-D), Expression(ymax)};
      boundPos = Expression(Atom("make-point"), temp).eval(env);
      boundLabel.setTextPosition(boundPos);
      result.push_back(boundLabel);

      if(m_tail.size() == 3){
        Expression OPTIONS = m_tail[2];
        // Add each option to the output

        Expression textItem, textPos, ffffff;
        for(auto &opt : OPTIONS.m_tail){


          temp.clear();

          if(opt.m_tail[0].head().asString() == "\"title\""){
            temp.push_back(Expression(Atom(xmiddle)));
            temp.push_back(Expression(Atom(ymax-A)));
          }
          else if(opt.m_tail[0].head().asString() == "\"abscissa-label\""){
            temp.push_back(Expression(Atom(xmiddle)));
            temp.push_back(Expression(Atom(ymin+A)));
          }
          else if(opt.m_tail[0].head().asString() == "\"ordinate-label\""){
            temp.push_back(Expression(Atom(xmin-B)));
            temp.push_back(Expression(Atom(ymiddle)));
          }

          textPos = Expression(Atom("make-point"), temp).eval(env);
          temp.clear();
          temp = { Expression(Atom(opt.m_tail[1].head().asString())) };
          textItem = Expression(Atom("make-text"), temp);

          temp = {Expression(Atom("\"position\"")), textPos, textItem};
          ffffff = Expression(Atom("\"set-property\""), temp);

          result.push_back(ffffff.eval(env));
        }

      }

      return Expression("CP", result);
    }
    else
      throw SemanticError("Error: invalid number of arguments to continuous plot");
}

sig_atomic_t global_status_flag = 0;

// this is a simple recursive version. the iterative version is more
// difficult with the ast data structure used (no parent pointer).
// this limits the practical depth of our AST
Expression Expression::eval(Environment & env){

  if(global_status_flag > 0){
    throw SemanticError("Error: interpreter kernal interupted");
  }
  else{
    if (m_head.asSymbol() == "list") {
      return handle_list(env);   
    }
    if(m_tail.empty()){
      return handle_lookup(m_head, env);
    }
    if(m_head.asSymbol() == "begin"){
      return handle_begin(env);
    }
    if(m_head.asSymbol() == "define"){
      return handle_define(env);
    }
    if(m_head.asSymbol() == "lambda"){
      return handle_lambda(env);
    }
    else if(m_head.asSymbol() == "apply"){
      return handle_apply(env);
    }
    if(m_head.asSymbol() == "map"){
      return handle_map(env);
    }
    if(m_head.asSymbol() == "set-property"){
      return handle_set_property(env);
    }
    if(m_head.asSymbol() == "get-property"){
      return handle_get_property(env);
    }
    if(m_head.asSymbol() == "discrete-plot"){
      return handle_discrete_plot(env);
    }
    if(m_head.asSymbol() == "continuous-plot"){
      return handle_cont_plot(env);
    }
  }

  std::vector<Expression> results;
  for(Expression::IteratorType it = m_tail.begin(); it != m_tail.end(); ++it){
    results.push_back(it->eval(env));
  }
  return apply(m_head, results, env);
}

std::ostream & operator<<(std::ostream & out, const Expression & exp){

  if(!exp.isEmpty()){
    if(!exp.head().isComplex()) {
      out << "(";
    }

    if (exp.isNone()) {
      out << exp.head().asString();
    }

    for(auto e = exp.tailConstBegin(); e != exp.tailConstEnd(); ++e){
      out << *e;
      if((e + 1) != exp.tailConstEnd()){
        out << " ";
      }
    }
    if(!exp.head().isComplex()) {
      out << ")";
    }
  }

  return out;
}

bool Expression::operator==(const Expression & exp) const noexcept{

  bool result = (m_head == exp.m_head);

  result = result && (m_tail.size() == exp.m_tail.size());

  if(result){
    for(auto lefte = m_tail.begin(), righte = exp.m_tail.begin();
	(lefte != m_tail.end()) && (righte != exp.m_tail.end());
	++lefte, ++righte){
      result = result && (*lefte == *righte);
    }
  }

  return result;
}

bool operator!=(const Expression & left, const Expression & right) noexcept{

  return !(left == right);
}

std::string Expression::toString() const noexcept{

  std::ostringstream out;

  if(this->isEmpty()){
    out << "NONE";
  }
  else {
    if(!this->head().isComplex()) {
      out << "(";
    }

    out << this->head().asString();

    for(auto e = this->tailConstBegin(); e != this->tailConstEnd(); ++e){
      out << *e;
      if((e + 1) != this->tailConstEnd()){
        out << " ";
      }
    }
    if(!this->head().isComplex()) {
      out << ")";
    }
  }

  return out.str();
}

std::string Expression::getProperties() const {

    std::ostringstream out;

    for (auto &p : m_properties) {
      out << p.first << " " << p.second.toString() << "\n";
    }

    return out.str();

}

bool Expression::isPoint() const noexcept{
 for(auto &p : m_properties){
    if(p.first.compare("object-name")){
      return p.second == Expression(Atom("\"point\""));
    }
  }
  return false;
}

bool Expression::isLine() const noexcept{
  for(auto &p : m_properties){
    if(p.first.compare("object-name")){
      return p.second == Expression(Atom("\"line\""));
    }
  }
  return false;
}

bool Expression::isText() const noexcept{
  const std::string target_property("object-name");
  for(auto &p : m_properties){
    if(p.first.compare(target_property)){
      return p.second == Expression(Atom("\"text\""));
    }
  }
  return false;
}

std::tuple<double, double, double, double, bool> Expression::getTextProperties() const noexcept{
  double sf = 1;
  if(m_properties.find("\"text-scale\"") != m_properties.end()) {
    sf = m_properties.at("\"text-scale\"").head().asNumber();
    if(sf < 1)
      sf = 1;
  }

  double rot = 0;
  if(m_properties.find("\"text-rotation\"") != m_properties.end()) {
    rot = m_properties.at("\"text-rotation\"").head().asNumber();
  }

  if(m_properties.find("\"position\"") != m_properties.end()){
    Expression point = m_properties.at("\"position\"");
    double x = point.getPointCoordinates().first;
    double y = point.getPointCoordinates().second;
    return {x, y, sf, rot, true};
  }

  // Error case
  return {0, 0, 1, 0, false};
}

std::vector<Expression> Expression::asVector() const noexcept {
    std::vector<Expression> result;

    if(!m_head.isNone()){
      result.push_back(m_head);
    }
    for(auto e : m_tail){
      result.emplace_back(e);
    }
    return result;
}

double Expression::getNumericalProperty(std::string prop) const noexcept {
  double size_value = -1;
  if(m_properties.find(prop) != m_properties.end()){
    Expression point_size = m_properties.at(prop);
    size_value = point_size.head().asNumber();
  }
  return size_value;
}

std::pair<double, double> Expression::getPointCoordinates() const noexcept {
  double x = 0.0;
  double y = 0.0;
  
  if (m_properties.find("object-name") != m_properties.end()) {
    if (m_properties.at("object-name") == Expression(Atom("point"))) {
      x = m_tail[0].head().asNumber();
      x = m_tail[1].head().asNumber();
    }
  }

  std::pair<double, double> result = {x, y};
  return result;
}

void Expression::setLineThickness(double thique) noexcept{
  if(m_properties.find("\"thickness\"")!=m_properties.end()){
    m_properties["\"thickness\""] = Expression(thique);
  }
}

void Expression::setPointSize(double uWu) noexcept{
  if(m_properties.find("\"size\"")!=m_properties.end()){
    m_properties["\"size\""] = Expression(uWu);
  }
}

void Expression::setTextPosition(Expression point, double rot) noexcept{
  if(m_properties.find("\"position\"")!=m_properties.end()){
    assert(point.isPoint());
    m_properties["\"position\""] = point;
  }
  if(m_properties.find("\"text-rotation\"")!=m_properties.end()){
    m_properties["\"text-rotation\""] = Expression(rot * std::atan(1)*4 / 180);
  }
}
