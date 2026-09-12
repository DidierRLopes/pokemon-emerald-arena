import type { Metadata } from "next";
import { headers } from "next/headers";
import "./globals.css";

export async function generateMetadata(): Promise<Metadata> {
  const request = await headers();
  const host = request.get("x-forwarded-host") || request.get("host") || "localhost:3000";
  const protocol = host.startsWith("localhost") || host.startsWith("127.0.0.1") ? "http" : "https";
  const origin = new URL(`${protocol}://${host}`);
  const title = "Emerald Arena — Pokémon Emerald. In real time.";
  const description = "Move. Dodge. Attack. Break the arena. Real-time battles inside Pokémon Emerald. Download the playable GBA demo.";
  const image = new URL("/og.png", origin).href;
  return {
    title, description, metadataBase: origin,
    openGraph: { title, description, type: "website", url: new URL("/arena", origin).href,
      images: [{ url: image, width: 1730, height: 909, alt: "Pokémon Emerald. In real time. Move. Dodge. Attack." }] },
    twitter: { card: "summary_large_image", title, description, images: [image] },
  };
}

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en">
      <body>
        {children}
      </body>
    </html>
  );
}
