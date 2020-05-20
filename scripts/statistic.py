#!python3
import os, sys
import psycopg2
import pandas as pd
import datetime as dt
import argparse as ap
import pytz
from sqlalchemy import create_engine

class eospg:
    def __init__(self,db_str,schema):
        try:
            self.conn = psycopg2.connect(db_str)
            self.conn.autocommit = True
            self.schema = schema
            self._db_info = {}
            for s in db_str.split(" "):
                ss = s.split("=")
                self._db_info[ss[0]] = ss[1].replace("'","")
        except:
            print("I am unable to connect to the database")

    def get_blocks(self, n):
        cur = self.conn.cursor()
        cur.execute("select * from %s.block_info order by block_num desc limit %s" % (self.schema,n))
        rows = cur.fetchall()
        colnames = [desc[0] for desc in cur.description]
        return pd.DataFrame(rows,columns = colnames)


    def get_blocks_after(self, start_time = dt.datetime.now() - dt.timedelta(days=1)):
        cur = self.conn.cursor()
        cur.execute("select * from %s.block_info where timestamp >= '%s'" % (self.schema,start_time.astimezone(pytz.utc)))
        rows = cur.fetchall()
        colnames = [desc[0] for desc in cur.description]
        return pd.DataFrame(rows,columns = colnames)

    def get_transactions_after(self, start_time = dt.datetime.now() - dt.timedelta(days=1)):
        cur = self.conn.cursor()
        cur.execute("select block_num from %s.block_info where timestamp >= '%s' order by block_num limit 1" % (self.schema,start_time.astimezone(pytz.utc)))
        rows = cur.fetchall()
        if(len(rows) == 0):return None
        start_block = rows[0][0]
        cur.execute("select * from %s.transaction_trace where block_num >= '%s'" % (self.schema,start_block))
        rows = cur.fetchall()
        colnames = [desc[0] for desc in cur.description]
        return pd.DataFrame(rows,columns = colnames)

    def get_action_after(self, start_time = dt.datetime.now() - dt.timedelta(days=1)):
        cur = self.conn.cursor()
        cur.execute("select block_num from %s.block_info where timestamp >= '%s' order by block_num limit 1" % (self.schema,start_time.astimezone(pytz.utc)))
        rows = cur.fetchall()
        if(len(rows) == 0):return None
        start_block = rows[0][0]
        cur.execute("select * from %s.action_trace where block_num >= '%s'" % (self.schema,start_block))
        rows = cur.fetchall()
        colnames = [desc[0] for desc in cur.description]
        return pd.DataFrame(rows,columns = colnames)


    def get_engine(self):
        return create_engine("postgresql://%s:%s@%s" % (self._db_info["user"],self._db_info["password"], self._db_info["host"]))




def main():
    parser = ap.ArgumentParser()
    parser.add_argument("-l",help="day, week, month",choices=["day","month","week"],required=True)
    parser.add_argument("--db",help="db string",default="user='postgres' host='localhost' password='123456'")
    parser.add_argument("--schema",help="schema",default="public")
    
    args = parser.parse_args()

    if args.l == "day":
        s_time = dt.datetime.now()-dt.timedelta(days=1)
    elif args.l == "week":
        s_time = dt.datetime.now()-dt.timedelta(weeks=1)
    elif args.l == "month":
        s_time = dt.datetime.now()-dt.timedelta(days=30)


    pg = eospg(args.db,args.schema)

    df_action = pg.get_action_after(s_time)
    assert(df_action is not None)

    print("start",dt.datetime.now())
    engin = pg.get_engine()

    print("write %s active contract statistic into table %s_statistic" % (args.l, args.l))
    ddf = df_action.groupby(["act_account"]).size().sort_values(ascending=False).head(5)
    ddf.to_sql("%s_active_contract_statistic" % args.l, engin, schema = args.schema, if_exists = "replace" )

    print("write %s active account statistic into table %s_statistic" % (args.l, args.l))
    ddf = df_action.groupby(["actor"]).size().sort_values(ascending=False).head(5)
    ddf.to_sql("%s_active_account_statistic" % args.l, engin, schema = args.schema, if_exists = "replace" )
    print("finish",dt.datetime.now())
    return 0

if __name__ == "__main__":
    sys.exit(main())
    pass

