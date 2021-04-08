/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_HINT_PARSER_MNT_WORKSPACE_PERCONA_SERVER_8_0_SOURCE_TARBALLS_TEST_PERCONA_SERVER_SQL_SQL_HINTS_YY_H_INCLUDED
# define YY_HINT_PARSER_MNT_WORKSPACE_PERCONA_SERVER_8_0_SOURCE_TARBALLS_TEST_PERCONA_SERVER_SQL_SQL_HINTS_YY_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int HINT_PARSER_debug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    MAX_EXECUTION_TIME_HINT = 258,
    RESOURCE_GROUP_HINT = 259,
    BKA_HINT = 260,
    BNL_HINT = 261,
    DUPSWEEDOUT_HINT = 262,
    FIRSTMATCH_HINT = 263,
    INTOEXISTS_HINT = 264,
    LOOSESCAN_HINT = 265,
    MATERIALIZATION_HINT = 266,
    NO_BKA_HINT = 267,
    NO_BNL_HINT = 268,
    NO_ICP_HINT = 269,
    NO_MRR_HINT = 270,
    NO_RANGE_OPTIMIZATION_HINT = 271,
    NO_SEMIJOIN_HINT = 272,
    MRR_HINT = 273,
    QB_NAME_HINT = 274,
    SEMIJOIN_HINT = 275,
    SUBQUERY_HINT = 276,
    DERIVED_MERGE_HINT = 277,
    NO_DERIVED_MERGE_HINT = 278,
    JOIN_PREFIX_HINT = 279,
    JOIN_SUFFIX_HINT = 280,
    JOIN_ORDER_HINT = 281,
    JOIN_FIXED_ORDER_HINT = 282,
    INDEX_MERGE_HINT = 283,
    NO_INDEX_MERGE_HINT = 284,
    SET_VAR_HINT = 285,
    SKIP_SCAN_HINT = 286,
    NO_SKIP_SCAN_HINT = 287,
    HASH_JOIN_HINT = 288,
    NO_HASH_JOIN_HINT = 289,
    HINT_ARG_NUMBER = 290,
    HINT_ARG_IDENT = 291,
    HINT_ARG_QB_NAME = 292,
    HINT_ARG_TEXT = 293,
    HINT_IDENT_OR_NUMBER_WITH_SCALE = 294,
    HINT_CLOSE = 295,
    HINT_ERROR = 296
  };
#endif
/* Tokens.  */
#define MAX_EXECUTION_TIME_HINT 258
#define RESOURCE_GROUP_HINT 259
#define BKA_HINT 260
#define BNL_HINT 261
#define DUPSWEEDOUT_HINT 262
#define FIRSTMATCH_HINT 263
#define INTOEXISTS_HINT 264
#define LOOSESCAN_HINT 265
#define MATERIALIZATION_HINT 266
#define NO_BKA_HINT 267
#define NO_BNL_HINT 268
#define NO_ICP_HINT 269
#define NO_MRR_HINT 270
#define NO_RANGE_OPTIMIZATION_HINT 271
#define NO_SEMIJOIN_HINT 272
#define MRR_HINT 273
#define QB_NAME_HINT 274
#define SEMIJOIN_HINT 275
#define SUBQUERY_HINT 276
#define DERIVED_MERGE_HINT 277
#define NO_DERIVED_MERGE_HINT 278
#define JOIN_PREFIX_HINT 279
#define JOIN_SUFFIX_HINT 280
#define JOIN_ORDER_HINT 281
#define JOIN_FIXED_ORDER_HINT 282
#define INDEX_MERGE_HINT 283
#define NO_INDEX_MERGE_HINT 284
#define SET_VAR_HINT 285
#define SKIP_SCAN_HINT 286
#define NO_SKIP_SCAN_HINT 287
#define HASH_JOIN_HINT 288
#define NO_HASH_JOIN_HINT 289
#define HINT_ARG_NUMBER 290
#define HINT_ARG_IDENT 291
#define HINT_ARG_QB_NAME 292
#define HINT_ARG_TEXT 293
#define HINT_IDENT_OR_NUMBER_WITH_SCALE 294
#define HINT_CLOSE 295
#define HINT_ERROR 296

/* Value type.  */



int HINT_PARSER_parse (class THD *thd, class Hint_scanner *scanner, class PT_hint_list **ret);

#endif /* !YY_HINT_PARSER_MNT_WORKSPACE_PERCONA_SERVER_8_0_SOURCE_TARBALLS_TEST_PERCONA_SERVER_SQL_SQL_HINTS_YY_H_INCLUDED  */
