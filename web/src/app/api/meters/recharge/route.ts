import { NextResponse } from 'next/server';
import { Pool } from 'pg';

const pool = new Pool({
  user: 'postgres',
  host: 'localhost',
  database: 'lampitex',
  password: 'mysecretpassword',
  port: 5432,
});

export async function POST(request: Request) {
  try {
    const { meter_id, amount } = await request.json();

    if (!meter_id || isNaN(parseFloat(amount))) {
      return NextResponse.json({ error: 'Invalid payload' }, { status: 400 });
    }

    const client = await pool.connect();
    
    await client.query(`
      UPDATE meters 
      SET pending_recharge = pending_recharge + $1
      WHERE id = $2
    `, [parseFloat(amount), meter_id]);

    client.release();

    return NextResponse.json({ success: true });
  } catch (error) {
    console.error('API /meters/recharge error:', error);
    return NextResponse.json({ error: 'Internal Server Error' }, { status: 500 });
  }
}
