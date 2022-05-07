#include "db_cxx.h"
#include <string>
#include <iostream>
#include "SQLParser.h"
#include "HeapTable.h"

using namespace hsql;

std::string execute (const SQLStatement *parseTree);

std::string parseInsertStm(InsertStatement * stm);

std::string parseCreateStm(CreateStatement * stm);

std::string parseColumnAttribute(ColumnDefinition * attr);

std::string parseSelectStm(SelectStatement * stm);

std::string parseExpression(Expr * exp);

std::string parseExprOperator(Expr * exp);

std::string tableExpr(TableRef * expr);

int main(int argc,char *argv[] ){
// Need 2 arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: ./Verano envi_path;\"\n");
        return -1;
    }

    // Create a Berkeley DB environment in the specified that directory
    DbEnv env(0U);
    env.set_message_stream(&std::cout);
	env.set_error_stream(&std::cerr);
    env.open(argv[1], DB_CREATE, 0);

    std::string input;
    std::cout << "SQL> ";
    std::getline(std::cin,input);

    while(input != "quit"){
        SQLParserResult *result = SQLParser::parseSQLString(input);
        if(result->isValid()){

            for(uint i = 0; i < result->size(); ++i){
                std::cout << execute(result->getStatement(i)) << std::endl;
            }
            
        }else{
            std::cout << "Invalid SQL: " << input << std::endl;
        }

        std::cout << "SQL> ";
        std::getline(std::cin,input);
    }

    return 0;
}

std::string execute(const SQLStatement *parseTree){
    std::string stm;

    switch(parseTree->type()){
        case hsql::kStmtSelect:
            stm += parseSelectStm((SelectStatement *) parseTree);
            break;
        case hsql::kStmtCreate:
            stm += parseCreateStm((CreateStatement *) parseTree);

            break;
        case hsql::kStmtInsert:
            stm += parseInsertStm((InsertStatement *) parseTree);
            break;
        default:
            return "Not implemented";
    }

    return stm;
}

std::string parseInsertStm(InsertStatement * stm){
    return "Not implemented";
}

std::string parseCreateStm(CreateStatement * stm){
    std::string res = "CREATE TABLE ";
    res += stm->tableName;
    res += "(";

    bool firstCol = true;
    
    //Iterate through each obj over smart ptr
    for(ColumnDefinition* attr : *stm->columns){
        if (firstCol) {
            firstCol = false;
        }else res += ", ";
        res += parseColumnAttribute(attr);
    }
    
    return res += ")";
}

std::string parseColumnAttribute(ColumnDefinition * attr){
    std::string res;
    res += attr->name;

    switch(attr->type){
        case ColumnDefinition::DataType::TEXT:
            res += " TEXT";
            break;
        case ColumnDefinition::DataType::INT:
            res += " INT";
            break;
        case ColumnDefinition::DataType::DOUBLE:
            res += " DOUBLE";
            break;
        default:
            res += " UNKNOWN";
            break;
    }
    return res;
}

std::string parseSelectStm(SelectStatement * stm){
    std::string res;
    res += "SELECT ";

    bool firstExp = true;
    for(Expr* exp : * stm->selectList){
        if (firstExp){
            firstExp = false;
        }else res += ", ";

        res += parseExpression(exp);
    }

    res += " FROM ";
    res += tableExpr(stm->fromTable);

    if (stm->whereClause){
        res += " WHERE ";
        res += parseExpression(stm->whereClause);
    }

    return res;
}

std::string parseExpression(Expr * exp){
    std::string res;
    switch (exp->type)
    {
        case kExprStar:
            res += "*";
            break;
        case kExprColumnRef:
            if(exp->table != nullptr){
                res += exp->table;
                res += ".";
            }
            // field name after "."
            res += exp->name;
            break;
        case kExprLiteralString:
            res += exp->name;
            break;
        case kExprLiteralFloat:
            res += exp->fval;
            break;
        case kExprLiteralInt:
            res += std::to_string(exp->ival);
            break;
        case kExprOperator:
            res += parseExprOperator(exp);
            break;
        default:
            res += "Not implemeneted";
            break;
    }

    return res;
}

std::string parseExprOperator(Expr * exp){
    std::string res;

    // first expression before simple exp operator
    if(exp->expr != nullptr){
        res += parseExpression(exp->expr);
    }

    switch (exp->opType)
    {
    // add simple expression operator
    case Expr::SIMPLE_OP:
        res += " ";
        res += exp->opChar;
        res += " ";
        break;
    case Expr::AND:
        res += " AND ";
        break;
    case Expr::OR:
        res += " OR ";
        break;
    case Expr::IN:
        res += " IN ";
        break;
    default:
        res += "Not implemented ";
        break;
    }

    // second expression after simple exp operator
    if(exp->expr2 != nullptr){
        res += parseExpression(exp->expr2);
    }
    return res;
}

std::string tableExpr(TableRef * expr){
    std::string res;

    switch(expr->type){
        case kTableName:
            res += expr->name;
            break;
        case kTableSelect:
            res += parseSelectStm(expr->select);
            break;
        case kTableJoin:
            if(expr->join->left != nullptr){
                res += tableExpr(expr->join->left);
                res += " LEFT JOIN ";
            }
            
            if(expr->join->right != nullptr){
                res += tableExpr(expr->join->right);
            }
            
            if(expr->join->condition != nullptr){
                res += " ON ";
                res += parseExpression(expr->join->condition);
            }

            break;
        
        // Pairing every row of A with every row of B
        case kTableCrossProduct:
            bool firstExp = true;
            for(TableRef* TBL : *expr->list){
                if(firstExp) firstExp = false;
                else res += ", ";
                res += tableExpr(TBL);
            }
            break;
    }

    if(expr->alias != NULL){
        res += " AS ";
        res += expr->alias;
    }
    
    return res;
}

