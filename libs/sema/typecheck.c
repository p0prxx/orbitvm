//===--------------------------------------------------------------------------------------------===
// orbit/sema/type.c
// This source is part of Orbit - Sema
//
// Created on 2017-10-10 by Amy Parent <amy@amyparent.com>
// Copyright (c) 2016-2018 Amy Parent <amy@amyparent.com>
// Available under the MIT License
// =^•.•^=
//===--------------------------------------------------------------------------------------------===
#include <stdio.h>
#include <orbit/ast/ast.h>
#include <orbit/ast/context.h>
#include <orbit/ast/diag.h>
#include <orbit/ast/builders.h>
#include <orbit/ast/traversal.h>
#include <orbit/csupport/tokens.h>
#include <orbit/mangling/mangle.h>
#include <orbit/sema/typecheck.h>
#include <orbit/utils/memory.h>

#include "sema_private.h"


/*
Type Analysis strategy

1. Go through type declarations, add to canonical type table.
2. Go through everything that can be typed easily (literals)

x. scoped analysis. build symbol tables, from global to nested scopes

*/

OrbitAST* sema_typeCopy(OrbitAST* src) {
    if(src == NULL) { return NULL; }
    OrbitAST* copy = NULL;
    
    switch (src->kind) {
    case ORBIT_AST_TYPEEXPR_ANY:
    case ORBIT_AST_TYPEEXPR_BOOL:
    case ORBIT_AST_TYPEEXPR_STRING:
    case ORBIT_AST_TYPEEXPR_NUMBER:
    case ORBIT_AST_TYPEEXPR_VOID:
        copy = orbit_astMakePrimitiveType(src->kind);
        break;
    case ORBIT_AST_TYPEEXPR_ARRAY:
        copy = orbit_astMakeArrayType(sema_typeCopy(src->typeExpr.arrayType.elementType));
        break;
    case ORBIT_AST_TYPEEXPR_MAP:
        copy = orbit_astMakeMapType(sema_typeCopy(src->typeExpr.mapType.keyType), 
                               sema_typeCopy(src->typeExpr.mapType.elementType));
        break;
    case ORBIT_AST_TYPEEXPR_FUNC:
        copy = orbit_astMakeFuncType(sema_typeCopy(src->typeExpr.funcType.returnType), 
                                sema_typeCopy(src->typeExpr.funcType.params));
        break;
    case ORBIT_AST_TYPEEXPR_USER:
        copy = orbit_astMakeUserTypePooled(src->typeExpr.userType.symbol);
        break;
    default:
        // TODO: throw error here, we're not working with a type expression.
        fprintf(stderr, "ERROR IN THERE\n");
        break;
    }
    copy->next = src->next ? ORCRETAIN(sema_typeCopy(src->next)) : NULL;
    return copy;
}

bool sema_typeEquals(OrbitAST* a, OrbitAST* b) {
    if(a == b) { return true; }
    if(!a && !b) { return true; }
    if(!a || !b) { return false; }
    if(!(a->kind & ASTTypeExprMask) || !(b->kind & ASTTypeExprMask)) { return false; }
    
    // TODO: refine, define and implement special `Any` semantics.
    if(a->kind != b->kind) { return false; }
    if(!sema_typeEquals(a->next, b->next)) { return false; }
    
    switch(a->kind) {
    case ORBIT_AST_TYPEEXPR_ANY:
    case ORBIT_AST_TYPEEXPR_BOOL:
    case ORBIT_AST_TYPEEXPR_STRING:
    case ORBIT_AST_TYPEEXPR_NUMBER:
    case ORBIT_AST_TYPEEXPR_VOID:
        return true;
        
    case ORBIT_AST_TYPEEXPR_ARRAY:
        return sema_typeEquals(a->typeExpr.arrayType.elementType,
                               b->typeExpr.arrayType.elementType);
    case ORBIT_AST_TYPEEXPR_MAP:
        return sema_typeEquals(a->typeExpr.mapType.elementType, b->typeExpr.mapType.elementType)
            && sema_typeEquals(a->typeExpr.mapType.keyType, b->typeExpr.mapType.keyType);
    case ORBIT_AST_TYPEEXPR_FUNC:
        return sema_typeEquals(a->typeExpr.funcType.returnType, b->typeExpr.funcType.returnType)
            && sema_typeEquals(a->typeExpr.funcType.params, b->typeExpr.funcType.params);
        
    case ORBIT_AST_TYPEEXPR_USER:
        return a->typeExpr.userType.symbol == b->typeExpr.userType.symbol;
    default:
        break;
    }
    // TODO: add unreachable flag here
    return NULL;
}

void sema_extractTypeAnnotations(OrbitASTContext* ctx, OrbitAST* decl, void* data) {
    if(!decl->varDecl.typeAnnotation) { return; }
    OCSema* sema = (OCSema*)data;
    
    if(decl->varDecl.typeAnnotation->kind == ORBIT_AST_TYPEEXPR_USER) {
        // If we have a user type we need to make sure it's been declared first
        OCStringID symbol = decl->varDecl.typeAnnotation->typeExpr.userType.symbol;
        
        OrbitAST* typeDecl = sema_lookupType(sema, symbol);
        if(!typeDecl) {
            OrbitSLoc loc = decl->varDecl.typeAnnotation->sourceRange.start;
            OrbitDiagID id = orbit_diagError(
                &ctx->diagnostics, loc, "unkown type '$0'", 1,
                ORBIT_DIAG_STRING(symbol)
            );
            orbit_diagAddSourceRange(id, decl->varDecl.typeAnnotation->sourceRange);
            return;
        }
    }
    decl->type = ORCRETAIN(decl->varDecl.typeAnnotation);
}

void sema_installUserTypes(OrbitASTContext* ctx, OrbitAST* typeDecl, void* data) {
    OCSema* sema = (OCSema*)data;
    OCStringID name = typeDecl->structDecl.name;
    
    if(orbit_rcMapGetP(&sema->typeTable, name)) {
        OrbitSLoc loc = typeDecl->structDecl.symbol.sourceLoc;
        orbit_diagError(
            &ctx->diagnostics, loc, "type '$0' was declared before", 1,
            ORBIT_DIAG_STRING(name)
        );
        //orbit_diagAddSourceRange(id, ...)
    } else {
        sema_declareType(sema, name, typeDecl);
    }
}

void sema_extractLiteralTypes(OrbitASTContext* ctx, OrbitAST* literal, void* data) {
    //OCSema* sema = (OCSema*)data;
    switch(literal->kind) {
    case ORBIT_AST_EXPR_CONSTANT_INTEGER:
    case ORBIT_AST_EXPR_CONSTANT_FLOAT:
        literal->type = ORCRETAIN(orbit_astMakePrimitiveType(ORBIT_AST_TYPEEXPR_NUMBER));
        break;
        
    case ORBIT_AST_EXPR_CONSTANT_STRING:
        literal->type = ORCRETAIN(orbit_astMakePrimitiveType(ORBIT_AST_TYPEEXPR_STRING));
        break;
    default:
        // TODO: signal error here, though that shouldn't really happen?
        break;
    }
}

void sema_extractFuncTypes(OrbitASTContext* ctx, OrbitAST* func, void* data) {
    
    OrbitAST* returnType = func->funcDecl.returnType ?
                        func->funcDecl.returnType :
                        orbit_astMakePrimitiveType(ORBIT_AST_TYPEEXPR_VOID);
    
    ASTListBuilder params;
    orbit_astListStart(&params);
    OrbitAST* param = func->funcDecl.params;
    while(param) {
        orbit_astListAdd(&params, sema_typeCopy(param->type));
        param = param->next;
    }
    func->type = ORCRETAIN(orbit_astMakeFuncType(returnType, orbit_astListClose(&params)));
    // TODO: We have a problem for overloads here. Do we need to do name mangling first? Or store
    // a collection of functions in the symbol table instead (and do mangling at code generation
    // time, since it's needed for VM operations?)
    func->funcDecl.mangledName = orbit_mangleFuncName(func);
    
    
    sema_declareSymbol((OCSema*)data, func->funcDecl.name, func->type);
}

void sema_checkExpression(OrbitASTContext* ctx, OrbitAST* ast, void* sema) {
    
}

void sema_checkBlock(OrbitASTContext* ctx, OrbitAST* block, OCSema* sema) {
    
}

void sema_doScopeAnalysis(OrbitASTContext* ctx, OrbitAST* func, void* data) {
    //OCSema* sema = (OCSema*)data;
    // TODO: sema should only contain the global scope right now.
    sema_checkBlock(ctx, func->funcDecl.body, (OCSema*)data);
}

void sema_runTypeAnalysis(OrbitASTContext* context) {
    // Initialise a Sema object
    OCSema sema;
    sema_init(&sema);
    
    orbit_astTraverse(
        context,
        orbit_astSimpleVisitor(&sema_installUserTypes, ORBIT_AST_DECL_STRUCT, &sema)
    );
    orbit_astTraverse(
        context,
        orbit_astSimpleVisitor(
            &sema_extractLiteralTypes,
            ORBIT_AST_EXPR_CONSTANT_INTEGER
          | ORBIT_AST_EXPR_CONSTANT_FLOAT
          | ORBIT_AST_EXPR_CONSTANT_STRING,
            &sema
        )
    );
    orbit_astTraverse(
        context,
        orbit_astSimpleVisitor(&sema_extractTypeAnnotations, ORBIT_AST_DECL_VAR, &sema)
    );
    
    orbit_astTraverse(
        context,
        orbit_astSimpleVisitor(&sema_extractFuncTypes, ORBIT_AST_DECL_FUNC, &sema)
    );
    
    orbit_astTraverse(
        context,
        orbit_astSimpleVisitor(&sema_doScopeAnalysis, ORBIT_AST_DECL_FUNC, &sema)
    );
    
    
    sema_deinit(&sema);
}