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
    const { meter_id, message } = await request.json();

    if (!meter_id || !message) {
      return NextResponse.json({ error: 'Invalid payload' }, { status: 400 });
    }

    const client = await pool.connect();
    
    await client.query(`
      UPDATE meters 
      SET pending_message = $1
      WHERE id = $2
    `, [message.substring(0, 16), meter_id]); // LCD is 16 chars wide

    client.release();

    return NextResponse.json({ success: true });
  } catch (error) {
    console.error('API /meters/message error:', error);
    return NextResponse.json({ error: 'Internal Server Error' }, { status: 500 });
  }
}
