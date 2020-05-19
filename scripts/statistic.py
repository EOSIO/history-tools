import psycopg2
import pandas as pd
import datetime as dt

class eospg:
    def __init__(self):
        try:
            self.conn = psycopg2.connect("user='postgres' host='localhost' password='123456'")
            self.schema = "testschema"
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
        cur.execute("select * from %s.block_info where timestamp >= '%s'" % (self.schema,start_time))
        rows = cur.fetchall()
        colnames = [desc[0] for desc in cur.description]
        return pd.DataFrame(rows,columns = colnames)

    def get_transactions_after(self, start_time = dt.datetime.now() - dt.timedelta(days=1)):
        cur = self.conn.cursor()
        cur.execute("select block_num from %s.block_info where timestamp >= '%s' order by block_num limit 1" % (self.schema,start_time))
        rows = cur.fetchall()
        start_block = rows[0][0]
        cur.execute("select * from %s.transaction_trace where block_num >= '%s'" % (self.schema,start_block))
        rows = cur.fetchall()
        colnames = [desc[0] for desc in cur.description]
        return pd.DataFrame(rows,columns = colnames)



def main():
    pg = eospg()

    print("block count in last 24 hours:")
    print(pg.get_blocks_after().shape[0])

    print("transaction count in last 24 hours:")
    print(pg.get_transactions_after().shape[0])


if __name__ == "__main__":
    main()
    pass

